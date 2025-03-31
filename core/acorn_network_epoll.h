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

#include <unordered_set>
#include <unordered_map>
#include <sys/epoll.h>
#include "acorn_module_http2.h"

const constexpr int EPOLL_MAX_EVENTS = 128;

class acorn_epoll
{
    int _mepollfd = -1;
    SSL_CTX *_ssl_ctx = nullptr;
    std::unordered_set<int> _fd_master_pool;
    std::unordered_map<int, std::string> _http_buffer;
    std::unordered_map<int, SSL *> _ssl_sessions;

public:
    acorn_epoll()
    {
        if (!acorn_http2::init_ssl(_ssl_ctx))
        {
            throw std::runtime_error("SSL initialization failed");
        }
    }

    void acorn_createEpoll()
    {
        _mepollfd = epoll_create1(0);
        if (_mepollfd == -1)
        {
            throw std::runtime_error("Failed to create epoll: " + std::string(strerror(errno)));
        }
    }

    void acorn_epollAddMSocket(std::vector<int> &msocks)
    {
        for (const auto &mfd : msocks)
        {
            struct epoll_event event = {EPOLLIN | EPOLLET, {.fd = mfd}};
            if (epoll_ctl(_mepollfd, EPOLL_CTL_ADD, mfd, &event) == -1)
            {
                throw std::runtime_error("epoll_ctl failed: " + std::string(strerror(errno)));
            }
            _fd_master_pool.insert(mfd);
        }
    }

    void acorn_process_http1(int cfd)
    {
        char buffer[512];
        ssize_t bytesRead = recv(cfd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0)
        {
            acorn_close(cfd);
            return;
        }
        buffer[bytesRead] = '\0';

        std::string response = "HTTP/1.1 301 Moved Permanently\r\n"
                               "Location: https://127.0.0.1/\r\n"
                               "Content-Length: 0\r\n\r\n";
        send(cfd, response.c_str(), response.length(), 0);
        acorn_close(cfd);
    }

    void acorn_process_http2(int cfd)
    {
        if (_ssl_sessions.find(cfd) == _ssl_sessions.end())
        {
            SSL *ssl = SSL_new(_ssl_ctx);
            SSL_set_fd(ssl, cfd);
            if (SSL_accept(ssl) <= 0)
            {
                std::cerr << "SSL accept failed: " << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
                acorn_close(cfd);
                return;
            }
            _ssl_sessions[cfd] = ssl;
            SSL_write(ssl, HTTP2_MAGIC, strlen(HTTP2_MAGIC));
            SSL_write(ssl, HTTP2_SETTINGS, 9);
        }

        SSL *ssl = _ssl_sessions[cfd];
        char buffer[512];
        int bytesRead = SSL_read(ssl, buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            acorn_close(cfd);
            return;
        }

        _http_buffer[cfd].append(buffer, bytesRead);
        std::string &data = _http_buffer[cfd];

        // Simplified frame parsing (assumes single GET request frame for now)
        if (data.length() >= 9)
        {
            uint32_t length = (data[0] << 16) | (data[1] << 8) | data[2];
            uint8_t type = data[3];
            uint32_t streamId = (data[5] & 0x7F) | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);

            if (type == 0x1 && data.length() >= 9 + length)
            { // HEADERS frame
                std::string payload = data.substr(9, length);
                std::string target = "/"; // Simplified: extract from payload in real HPACK
                std::string content = acorn_http2::serve_file(target);

                if (!content.empty())
                {
                    std::string headers = acorn_http2::build_headers_frame(streamId, content, target);
                    std::string dataFrame = acorn_http2::build_data_frame(streamId, content);
                    SSL_write(ssl, headers.c_str(), headers.length());
                    SSL_write(ssl, dataFrame.c_str(), dataFrame.length());
                }
                _http_buffer[cfd].clear();
            }
        }
    }

    void acorn_close(int cfd)
    {
        if (_ssl_sessions.count(cfd))
        {
            SSL_shutdown(_ssl_sessions[cfd]);
            SSL_free(_ssl_sessions[cfd]);
            _ssl_sessions.erase(cfd);
        }
        _http_buffer.erase(cfd);
        close(cfd);
    }

    void acorn_epollEventsReady()
    {
        extern bool running;
        while (running)
        {
            struct epoll_event events[EPOLL_MAX_EVENTS];
            int eventCount = epoll_wait(_mepollfd, events, EPOLL_MAX_EVENTS, -1);
            if (eventCount == -1 && errno != EINTR)
            {
                std::cerr << "epoll_wait failed: " << strerror(errno) << std::endl;
                continue;
            }

            for (int i = 0; i < eventCount; ++i)
            {
                int fd = events[i].data.fd;
                if (events[i].events & (EPOLLERR | EPOLLHUP))
                {
                    acorn_close(fd);
                }
                else if (events[i].events & EPOLLIN)
                {
                    if (_fd_master_pool.count(fd))
                    {
                        int clientfd = accept4(fd, nullptr, nullptr, SOCK_NONBLOCK);
                        if (clientfd == -1)
                            continue;
                        struct epoll_event ev = {EPOLLIN | EPOLLET, {.fd = clientfd}};
                        epoll_ctl(_mepollfd, EPOLL_CTL_ADD, clientfd, &ev);
                        if (fd == _fd_master_pool.find(80)->first)
                        {
                            acorn_process_http1(clientfd);
                        }
                        else
                        {
                            acorn_process_http2(clientfd);
                        }
                    }
                    else
                    {
                        if (_ssl_sessions.count(fd))
                        {
                            acorn_process_http2(fd);
                        }
                        else
                        {
                            acorn_process_http1(fd);
                        }
                    }
                }
            }
        }
    }

    ~acorn_epoll()
    {
        for (const auto &[fd, ssl] : _ssl_sessions)
        {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(fd);
        }
        if (_mepollfd != -1)
            close(_mepollfd);
        if (_ssl_ctx)
            SSL_CTX_free(_ssl_ctx);
    }
};

#endif