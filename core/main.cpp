/*
 *   Copyright 2024 Acorn
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

#include <iostream>
#include <csignal>
#include "acorn_network_socket.h"
#include "acorn_network_epoll.h"

bool running = true;

void signalHandler(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    running = false;
}

int main()
{
    signal(SIGTERM, signalHandler);
    signal(SIGQUIT, signalHandler);
    signal(SIGINT, signalHandler);

    std::cout << "Acorn Running..\n";

    acorn_socket listen;
    listen.addressPort.emplace_back("127.0.0.1", 80);  // HTTP/1.1 upgrade
    listen.addressPort.emplace_back("127.0.0.1", 443); // HTTP/2 TLS
    listen.acorn_listener();

    acorn_epoll epoll_init;
    try
    {
        epoll_init.acorn_createEpoll();
        epoll_init.acorn_epollAddMSocket(listen._msock);
        epoll_init.acorn_epollEventsReady();
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    std::cout << "Acorn Closed.\n";
    return 0;
}