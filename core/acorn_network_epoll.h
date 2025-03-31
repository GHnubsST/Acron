/*
*   Copyright @ 2024 Acorn
*
*   Licensed under the Apache License, Version 2.0 (the "License");
*   you may not use this file except in compliance with the License.
*   You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*   See the License for the specific language governing permissions and
*   limitations under the License.
*/

#ifndef ACORN_NETWORK_EPOLL_H
#define ACORN_NETWORK_EPOLL_H

const constexpr int EPOLL_MAX_EVENTS = 128;

class acorn_epoll {
    int _mepollfd = -1;
    std::unordered_set<int> _fd_master_pool;
    std::unordered_map<int, std::string> http_event_map;  // No mutex needed

public:
    acorn_epoll() {}

    void acorn_createEpoll() {
        _mepollfd = epoll_create1(0);
        if (_mepollfd == -1) {
            throw std::runtime_error("Failed to create epoll instance: " + std::string(strerror(errno)));
        }
    }

    void acorn_epollAddMSocket(std::vector<int> &msocks) {
        for (const auto& mfd : msocks) {
            struct epoll_event event;
            event.events = EPOLLIN | EPOLLET;
            event.data.fd = mfd;
            if (epoll_ctl(_mepollfd, EPOLL_CTL_ADD, mfd, &event) == -1) {
                throw std::runtime_error("epoll_ctl failed: " + std::string(strerror(errno)));
            }
            _fd_master_pool.insert(mfd);
        }
    }

    void acorn_HttpCreateEventMap(int bucket) {
        if (http_event_map.find(bucket) == http_event_map.end()) {
            http_event_map[bucket] = "";
        }
    }

    void acorn_HttpUpdateEventMap(int bucket, const std::string& httpString) {
        if (!httpString.empty()) {
            http_event_map[bucket] = httpString;
        }
    }

    void acorn_HttpRemoveEventMap(int bucket) {
        http_event_map.erase(bucket);
    }

    void acorn_HttpProcess(const int cfd) {
        char temp_buffer[MIN_BUFFER_SIZE];
        std::string readStr;

        // Read until headers are complete
        while (true) {
            ssize_t bytesRead = recv(cfd, temp_buffer, sizeof(temp_buffer) - 1, 0);
            if (bytesRead > 0) {
                temp_buffer[bytesRead] = '\0';
                readStr.append(temp_buffer, bytesRead);
                if (readStr.find("\r\n\r\n") != std::string::npos) {
                    break;  // Headers complete
                }
                if (readStr.length() > MAX_HEADER_LENGTH) {
                    acorn_HttpUpdateEventMap(cfd, "HTTP/1.1 431 Request Header Fields Too Large\r\n\r\n");
                    break;
                }
            } else if (bytesRead == 0 || (bytesRead == -1 && errno != EAGAIN)) {
                acorn_epollGraceClose(cfd, SHUT_RDWR);
                return;
            } else {
                break;  // EAGAIN, retry later
            }
        }

        if (!readStr.empty()) {
            acorn_HttpUpdateEventMap(cfd, readStr);
            std::string payload = http_event_map[cfd];
            std::string_view httpRequest(payload);
            std::ostringstream responseStream = acorn_header_parser(httpRequest);
            std::string response = responseStream.str();

            if (!response.empty()) {
                size_t totalBytesSent = 0;
                const char* responseData = response.c_str();
                size_t responseLength = response.length();

                while (totalBytesSent < responseLength) {
                    ssize_t bytesSent = send(cfd, responseData + totalBytesSent, responseLength - totalBytesSent, 0);
                    if (bytesSent == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            continue;
                        }
                        break;
                    }
                    totalBytesSent += bytesSent;
                }

                // Check for keep-alive
                if (payload.find("Connection: keep-alive") != std::string::npos ||
                    (payload.find("HTTP/1.1") != std::string::npos && payload.find("Connection: close") == std::string::npos)) {
                    // Keep connection open, reset buffer
                    acorn_HttpUpdateEventMap(cfd, "");
                } else {
                    acorn_epollGraceClose(cfd, SHUT_RDWR);
                }
            }
        }
    }

    void acorn_epollGraceClose(int cfd, const int& type) {
        acorn_HttpRemoveEventMap(cfd);
        if (type == SHUT_RDWR || type == SHUT_RD || type == SHUT_WR) {
            if (shutdown(cfd, type) == -1) {
                std::cerr << "Failed to shutdown socket: " << strerror(errno) << std::endl;
            }
        }
        if (type == SHUT_RDWR) {
            while (close(cfd) == -1 && errno == EINTR) {}
        }
    }

    void acorn_epollEventsReady() {
        while (running) {
            struct epoll_event events[EPOLL_MAX_EVENTS];
            int eventCount = epoll_wait(_mepollfd, events, EPOLL_MAX_EVENTS, -1);
            if (eventCount == -1 && errno != EINTR) {
                throw std::runtime_error("Failed to epoll wait: " + std::string(strerror(errno)));
            }

            for (int i = 0; i < eventCount; ++i) {
                const int cfd = events[i].data.fd;
                if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    acorn_epollGraceClose(cfd, SHUT_RDWR);
                } else if (events[i].events & EPOLLIN) {
                    if (_fd_master_pool.count(cfd)) {
                        const int clientfd = accept4(cfd, nullptr, nullptr, SOCK_NONBLOCK);
                        if (clientfd == -1) {
                            std::cerr << "Accept failed: " << strerror(errno) << std::endl;
                            continue;
                        }
                        struct epoll_event event = {EPOLLIN | EPOLLRDHUP | EPOLLET, {.fd = clientfd}};
                        if (epoll_ctl(_mepollfd, EPOLL_CTL_ADD, clientfd, &event) == -1) {
                            acorn_epollGraceClose(clientfd, SHUT_RDWR);
                            continue;
                        }
                        acorn_HttpCreateEventMap(clientfd);
                    } else {
                        acorn_HttpProcess(cfd);
                    }
                }
            }
        }
    }

    ~acorn_epoll() {
        for (const auto& pair : http_event_map) {
            close(pair.first);
            std::cout << "Closing Client fd: " << pair.first << std::endl;
        }
        if (_mepollfd != -1) {
            close(_mepollfd);
            std::cout << "Epoll instance closed successfully" << std::endl;
        }
    }
};
#endif