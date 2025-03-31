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


#ifndef ACORN_HTTP_H
#define ACORN_HTTP_H

#include <fstream>

constexpr const char* HTTP_200 = "HTTP/1.1 200 OK";
constexpr const char* HTTP_400 = "HTTP/1.1 400 Bad Request";
constexpr const char* HTTP_404 = "HTTP/1.1 404 Not Found";
constexpr const char* HTTP_405 = "HTTP/1.1 405 Method Not Allowed";
constexpr const char* HTTP_414 = "HTTP/1.1 414 URI Too Long";
constexpr const char* HTTP_431 = "HTTP/1.1 431 Request Header Fields Too Large";
constexpr const char* HTTP_500 = "HTTP/1.1 500 Internal Server Error";
constexpr const char* HTTP_505 = "HTTP/1.1 505 HTTP Version Not Supported";

constexpr const ssize_t MIN_BUFFER_SIZE = 512;
constexpr const ssize_t MAX_HEADER_LENGTH = 8000;
constexpr const ssize_t MAX_HEADER_URL_LENGTH = 4000;

inline const char* acorn_header_rfc_validation(const std::string_view& view_http_buffer) {
    size_t headerEnd = view_http_buffer.find("\r\n\r\n");
    if (headerEnd == std::string_view::npos || headerEnd > MAX_HEADER_LENGTH) {
        return HTTP_431;
    }

    size_t firstLineEnd = view_http_buffer.find("\r\n");
    if (firstLineEnd == std::string_view::npos) {
        return HTTP_400;
    }

    std::string_view firstLine = view_http_buffer.substr(0, firstLineEnd);
    size_t firstSpace = firstLine.find(' ');
    size_t secondSpace = firstLine.rfind(' ');
    if (firstSpace == std::string_view::npos || secondSpace == std::string_view::npos || firstSpace == secondSpace) {
        return HTTP_400;
    }

    std::string_view method = firstLine.substr(0, firstSpace);
    std::string_view target = firstLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    std::string_view httpVersion = firstLine.substr(secondSpace + 1);

    if (method != "GET" && method != "POST" && method != "HEAD") {
        return HTTP_405;
    }
    if (target.length() > MAX_HEADER_URL_LENGTH) {
        return HTTP_414;
    }
    if (httpVersion != "HTTP/1.1" && httpVersion != "HTTP/1.0") {
        return HTTP_505;
    }
    return nullptr;
}

std::ostringstream acorn_header_parser(const std::string_view& view_http_buffer) {
    std::ostringstream responseStream;
    const char* responseCode = acorn_header_rfc_validation(view_http_buffer);

    if (responseCode != nullptr) {
        responseStream << responseCode << "\r\n";
        responseStream << "Server: Acorn HTTP Server\r\n";
        responseStream << "Content-Length: 0\r\n";
        responseStream << "\r\n";
        return responseStream;
    }

    size_t firstLineEnd = view_http_buffer.find("\r\n");
    std::string_view firstLine = view_http_buffer.substr(0, firstLineEnd);
    size_t firstSpace = firstLine.find(' ');
    size_t secondSpace = firstLine.rfind(' ');
    std::string_view method = firstLine.substr(0, firstSpace);
    std::string_view target = firstLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);

    // Sanitize target to prevent directory traversal
    std::string filePath = "html" + std::string(target);
    if (filePath.find("..") != std::string::npos) {
        filePath = "html/index.html";  // Default to index on suspicious input
    }
    if (filePath == "html/" || filePath == "html") {
        filePath = "html/index.html";
    }

    std::ifstream file(filePath, std::ios::binary);
    std::string content;
    if (!file.is_open()) {
        responseStream << HTTP_404 << "\r\n";
        responseStream << "Server: Acorn HTTP Server\r\n";
        responseStream << "Content-Length: 0\r\n";
        responseStream << "\r\n";
    } else {
        std::ostringstream contentStream;
        contentStream << file.rdbuf();
        content = contentStream.str();
        file.close();

        std::time_t currentTime = std::time(nullptr);
        char timeStr[80];
        std::strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&currentTime));

        responseStream << HTTP_200 << "\r\n";
        responseStream << "Server: Acorn HTTP Server\r\n";
        responseStream << "Date: " << timeStr << "\r\n";
        responseStream << "Content-Length: " << content.length() << "\r\n";
        responseStream << "Content-Type: text/html\r\n";  // Adjust based on file extension if needed
        responseStream << "Connection: keep-alive\r\n";   // Default for HTTP/1.1
        if (method != "HEAD") {
            responseStream << "\r\n" << content;
        } else {
            responseStream << "\r\n";
        }
    }

    return responseStream;
}
#endif
