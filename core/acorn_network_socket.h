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

#ifndef ACORN_NETWORK_SOCKET_H
#define ACORN_NETWORK_SOCKET_H

class acorn_socket {
  
public:
    std::vector<int> _msock;
    std::vector<std::pair<std::string, uint16_t>> addressPort;

    // Create a constructor for a cleanup destructor, dont use throw within constructor
    acorn_socket() {}

private:
    const int acorn_createSocket() {
        const int msock = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
        if (msock == -1) {
            throw std::runtime_error(std::string(strerror(errno)));
        }
        return msock;
    }

    void acorn_setSocket_Reuse(const int &mfd) {
        const int optval = 1;
        const int setSockError = setsockopt(mfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval));
        if (setSockError == -1) {
            throw std::runtime_error(std::string(strerror(errno))); 
        }
    }

    void acorn_bindSocket(const int &mfd, const std::pair<std::string, uint16_t> &addrPort) {
        struct sockaddr_in6 addr6;
        std::memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(addrPort.second);
        std::string address = addrPort.first;                 

        if (address == "::/0" || address == "0.0.0.0") {
            addr6.sin6_addr = in6addr_any;
        } 

        if (address.find('.') != std::string::npos) {
            size_t pos = address.find('/');
            address = address.substr(0, pos);
            address = "::ffff:" + address;
        }   

        if (inet_pton(AF_INET6, address.c_str(), &addr6.sin6_addr) <= 0) {
            throw std::runtime_error(std::string(strerror(errno)));
        }

        if (bind(mfd, reinterpret_cast<struct sockaddr*>(&addr6), sizeof(addr6)) == -1) {
            throw std::runtime_error(std::string(strerror(errno)));
        } 
    }

    void acorn_listenSocket(const int &mfd) {
        int backlog = std::min(128, static_cast<int>(SOMAXCONN));
        if (listen(mfd, backlog) == -1) {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    }

public:
    void acorn_listener() {
        for (const auto& addrPort : addressPort) {
            int Msock = -1;
            try {
                Msock = acorn_createSocket();
                std::cout << "Socket Created" << std::endl;
            }
            catch (const std::runtime_error& e) {
                std::cerr << e.what() << std::endl;
                close(Msock);
                continue;
            }
            catch(...) 
            {
                std::cout << "Undefined Error on Socket Creation" << std::endl;
            }
            try {
                acorn_setSocket_Reuse(Msock);
                std::cout << "Socket Set" << std::endl;
            }
            catch (const std::runtime_error& e) {
                std::cerr << e.what() << std::endl;
                continue;
            }
            catch(...) 
            {
                std::cout << "Undefined Error on Socket Set" << std::endl;
            }
            try {
                acorn_bindSocket(Msock, addrPort);
                std::cout << "Socket Bound" << std::endl;
            }
            catch (const std::runtime_error& e) {
                std::cerr << e.what() << std::endl;
                continue;
            }
            catch(...) 
            {
                std::cout << "Undefined Error on Socket Bound" << std::endl;
            }
            try {
                acorn_listenSocket(Msock);
                std::cout << "Socket listening" << std::endl;
            }
            catch (const std::runtime_error& e) {
                std::cerr << e.what() << std::endl;
                continue;
            }
            catch(...) 
            {
                std::cout << "Undefined Error on Socket listener" << std::endl;
            }
            _msock.push_back(Msock);             
            std::cout << "socket: " << Msock << " Address: " << addrPort.first << " Port: " << addrPort.second << std::endl;
        } 
    }     

    ~acorn_socket() {
        for (const auto& Sock : _msock)
        {
            if (Sock != -1) {
                close(Sock);
                std::cout << "Master socket closed successfully: " << Sock << std::endl;
            }
        }
    }
};

#endif
