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

bool running = true;

#include "acorn_headers.h"
#include "acorn_module_http.h"
#include "acorn_network_socket.h"
#include "acorn_network_epoll.h"

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    running = false;
}

int main() {
    signal(SIGTERM, signalHandler);
    signal(SIGQUIT, signalHandler);
    signal(SIGINT, signalHandler);

    std::cout << "Acorn Running.." << std::endl;

    acorn_socket listen;
    listen.addressPort.emplace_back(std::make_pair("127.0.0.1", 8080));
    listen.addressPort.emplace_back(std::make_pair("::1", 8080));
    listen.acorn_listener();

    acorn_epoll epoll_init;
    try {
        epoll_init.acorn_createEpoll();
        epoll_init.acorn_epollAddMSocket(listen._msock);
        epoll_init.acorn_epollEventsReady();
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
    catch(...) 
    {
        std::cout << "Undefined Error on startup" << std::endl;
    }

    std::cout << "Acorn Closed." << std::endl;

    return 0;
};