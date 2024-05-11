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
    std::unordered_set<int> _fd_master_pool = {};     
    std::unordered_map<int, std::string> http_event_map = {};
    std::mutex map_mutex; 

public: 
    acorn_epoll() {}

    void acorn_createEpoll() {
        _mepollfd = epoll_create1(0);
        if (_mepollfd == -1) {
            const int err = errno;
            throw std::runtime_error("Failed to create epoll instance: " + std::string(strerror(err)));
        }
    }

    void acorn_epollAddMSocket(std::vector<int> &msocks) {
        for (const auto& mfd : msocks) {
            struct epoll_event event;
            event.events = EPOLLIN | EPOLLET;
            event.data.fd = mfd;
            if (epoll_ctl(_mepollfd, EPOLL_CTL_ADD, mfd, &event) == -1) {
                const int err = errno;
                throw std::runtime_error("epoll_ctl failed: " + std::string(strerror(err)));
            }
            _fd_master_pool.insert(mfd);
        }
    }

    void acorn_HttpCreateEventMap(int bucket) {
        std::lock_guard<std::mutex> lock(map_mutex);
        if (http_event_map.find(bucket) == http_event_map.end()) {
            http_event_map[bucket] = ""; 
        } 
    }

    void acorn_HttpUpdateEventMap(int bucket, const std::string& httpString) {
        std::lock_guard<std::mutex> lock(map_mutex);
        auto it = http_event_map.find(bucket);

        if(it != http_event_map.end()) {
            if(!httpString.empty()) {
                http_event_map[bucket] = httpString;
            }
        }
    }

    void acorn_HttpRemoveEventMap(int bucket) {
        std::lock_guard<std::mutex> lock(map_mutex);
        http_event_map.erase(bucket);
    }

    void acorn_HttpProcess(const int cfd) {
        try {
            char temp_buffer[MIN_BUFFER_SIZE];
            ssize_t totalBytesRead = 0;
            std::string readStr = "";

            while (true)
            {
                memset(temp_buffer, 0, MIN_BUFFER_SIZE);
                ssize_t bytesRead = recv(cfd, temp_buffer, sizeof(temp_buffer), 0);
                if (bytesRead > 0) {
                    totalBytesRead += bytesRead;
                    readStr.append(temp_buffer, bytesRead);
                    if(totalBytesRead > MAX_HEADER_LENGTH) {
                        if(acorn_header_rfc_validation(readStr) != nullptr) {
                            break;
                        }
                    }
                    break;
                } else if(bytesRead == -1) {
                    const int err = errno;
                    if(err == EINTR) { // Interrupted by a signal before any data was received
                        continue;
                    } else if (err == EAGAIN || err == EWOULDBLOCK) { // Non-blocking socket operation should be retried later as no data is available now
                        break;
                    } else if (err == EBADF || err == EFAULT || err == EINVAL || err == ENOMEM || err == ENOTCONN || err == ENOTSOCK){
                        throw std::runtime_error("Receive failed: " + std::string(strerror(err)));
                    }
                    break;
                }
            }
            std::cout << readStr << std::endl;
            acorn_HttpUpdateEventMap(cfd, readStr);

        } catch (const std::runtime_error& e) {
            acorn_epollGraceClose(cfd, SHUT_RDWR);
            std::cerr << e.what() << std::endl;
        }
        catch(...) 
        {
            std::cout << "Undefined Error on Epoll Shutdown" << std::endl;
        }

        try {         
            if (!http_event_map[cfd].empty()) {
                std::string payload = http_event_map[cfd];
                std::string_view httpRequest(payload);
                std::ostringstream responseStream = acorn_header_parser(httpRequest);
                std::string response = responseStream.str();

                if (!response.empty()) {

                    std::cout << response << std::endl;

                    size_t totalBytesSent = 0;
                    size_t responseLength = response.length();
                    const char* responseData = response.c_str();

                    while (totalBytesSent < responseLength) {
                        ssize_t bytesSent = send(cfd, responseData + totalBytesSent, responseLength - totalBytesSent, 0);
                        if (bytesSent == -1) {
                            const int err = errno;
                            if (err == EAGAIN || err == EWOULDBLOCK) {
                                continue;
                            } else {
                                break; 
                            }
                        }
                        totalBytesSent += bytesSent;
                    }
                }
            }
        } 
        catch (const std::runtime_error& e) {
            acorn_epollGraceClose(cfd, SHUT_RDWR);
            std::cerr << e.what() << std::endl;
        } 
        catch(...) 
        {
            std::cout << "Undefined Error on Epoll Shutdown" << std::endl;
        } 
   }

    void acorn_epollGraceClose(int cfd, const int& type) {
        try {
            acorn_HttpRemoveEventMap(cfd);
            if (type == SHUT_RDWR || type == SHUT_RD || type == SHUT_WR) {
                if(shutdown(cfd, type) == -1) { // Shutdown failed
                    const int err = errno;
                    throw std::runtime_error("Failed to shutdown socket: " + std::string(strerror(err)));
                }
            }
            if(type == SHUT_RDWR) {
                while(true) {    
                    if(close(cfd) == -1) { // Close failed
                        const int err = errno;
                        if(err == EINTR) { // The call was interrupted by a signal handler before any events were received.
                            continue;
                        } else {
                            throw std::runtime_error("Unexpected error while closing file descriptor: " + std::string(strerror(err)));
                        }
                    } else {
                        break;
                    }  
                } 
            }
        } 
        catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }
        catch(...) 
        {
            std::cout << "Undefined Error on Epoll Shutdown" << std::endl;
        }
    }

    void acorn_epollEventsReady() {
        while(running) {
            struct epoll_event events[EPOLL_MAX_EVENTS];
            int eventCount = epoll_wait(_mepollfd, events, EPOLL_MAX_EVENTS, -1);
            if (eventCount == -1) {
                const int err = errno;
                if(err == EINTR) { // The call was interrupted by a signal handler before any events were received.
                    continue;
                } else {
                    throw std::runtime_error("Failed to epoll wait: " + std::string(strerror(err)));
                }
            }

            for (int i = 0; i < eventCount; ++i) {
                const int cfd = events[i].data.fd;
                
                if (events[i].events & EPOLLERR) {
                    std::cerr << "Error on socket: " << cfd << std::endl;
                    acorn_epollGraceClose(cfd, SHUT_RDWR);
                } else if (events[i].events & EPOLLHUP) {
                    std::cerr << "Local hang-up on socket: " << cfd << std::endl;
                    acorn_epollGraceClose(cfd, SHUT_RDWR);
                } else if (events[i].events & EPOLLRDHUP) {
                    std::cerr << "Remote hang-up on socket: " << cfd << std::endl;
                    acorn_epollGraceClose(cfd, SHUT_RDWR);
                } else {
                    if ((events[i].events & EPOLLIN)) {
                        if (_fd_master_pool.find(events[i].data.fd) != _fd_master_pool.end()) {
                            const int clientfd = accept4(events[i].data.fd, nullptr, nullptr, SOCK_NONBLOCK);
                            if (clientfd == -1) {
                                const int err = errno;
                                if (err == EBADF || err == EFAULT || err == EINVAL || err == EMFILE || err == ENFILE || 
                                    err == ENOBUFS || err == ENOMEM || err == ENOTSOCK || err == EOPNOTSUPP || err == EPROTO || err == EPERM) {
                                    throw std::runtime_error("Failed to accept new connection: " + std::string(strerror(err)));
                                }
                                continue;
                            }

                            struct epoll_event event = {EPOLLIN | EPOLLRDHUP | EPOLLET, {.fd = clientfd}};
                            if (epoll_ctl(_mepollfd, EPOLL_CTL_ADD, clientfd, &event) == -1) {
                                const int err = errno;
                                if (err == EEXIST || err == EBADF || err == EINVAL || err == ELOOP || err == ENOENT || err == ENOMEM || err == ENOSPC || err == EPERM) {
                                    acorn_epollGraceClose(clientfd, SHUT_RDWR);
                                    throw std::runtime_error("epoll_ctl failed: " + std::string(strerror(err)));
                                }
                                acorn_epollGraceClose(clientfd, SHUT_RDWR);
                                continue;
                            } 
                            acorn_HttpCreateEventMap(clientfd);
                        } else {
                            // Process data
                            acorn_HttpProcess(cfd);
                        }
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