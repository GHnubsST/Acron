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

// Informational responses
constexpr const char* HTTP_100 = "HTTP/1.1 100 Continue";
constexpr const char* HTTP_101 = "HTTP/1.1 101 Switching Protocols";
constexpr const char* HTTP_102 = "HTTP/1.1 102 Processing";
constexpr const char* HTTP_103 = "HTTP/1.1 103 Early Hints";

// Successful responses
constexpr const char* HTTP_200 = "HTTP/1.1 200 OK";
constexpr const char* HTTP_201 = "HTTP/1.1 201 Created";
constexpr const char* HTTP_202 = "HTTP/1.1 202 Accepted";
constexpr const char* HTTP_203 = "HTTP/1.1 203 Non-Authoritative Information";
constexpr const char* HTTP_204 = "HTTP/1.1 204 No Content";
constexpr const char* HTTP_205 = "HTTP/1.1 205 Reset Content";
constexpr const char* HTTP_206 = "HTTP/1.1 206 Partial Content";
constexpr const char* HTTP_207 = "HTTP/1.1 207 Multi-Status";
constexpr const char* HTTP_208 = "HTTP/1.1 208 Already Reported";
constexpr const char* HTTP_226 = "HTTP/1.1 226 IM Used";

// Redirection messages
constexpr const char* HTTP_300 = "HTTP/1.1 300 Multiple Choices";
constexpr const char* HTTP_301 = "HTTP/1.1 301 Moved Permanently";
constexpr const char* HTTP_302 = "HTTP/1.1 302 Found";
constexpr const char* HTTP_303 = "HTTP/1.1 303 See Other";
constexpr const char* HTTP_304 = "HTTP/1.1 304 Not Modified";
constexpr const char* HTTP_305 = "HTTP/1.1 305 Use Proxy";
constexpr const char* HTTP_307 = "HTTP/1.1 307 Temporary Redirect";
constexpr const char* HTTP_308 = "HTTP/1.1 308 Permanent Redirect";

// Client error responses
constexpr const char* HTTP_400 = "HTTP/1.1 400 Bad Request";
constexpr const char* HTTP_401 = "HTTP/1.1 401 Unauthorized";
constexpr const char* HTTP_402 = "HTTP/1.1 402 Payment Required";
constexpr const char* HTTP_403 = "HTTP/1.1 403 Forbidden";
constexpr const char* HTTP_404 = "HTTP/1.1 404 Not Found";
constexpr const char* HTTP_405 = "HTTP/1.1 405 Method Not Allowed";
constexpr const char* HTTP_406 = "HTTP/1.1 406 Not Acceptable";
constexpr const char* HTTP_407 = "HTTP/1.1 407 Proxy Authentication Required";
constexpr const char* HTTP_408 = "HTTP/1.1 408 Request Timeout";
constexpr const char* HTTP_409 = "HTTP/1.1 409 Conflict";
constexpr const char* HTTP_410 = "HTTP/1.1 410 Gone";
constexpr const char* HTTP_411 = "HTTP/1.1 411 Length Required";
constexpr const char* HTTP_412 = "HTTP/1.1 412 Precondition Failed";
constexpr const char* HTTP_413 = "HTTP/1.1 413 Payload Too Large";
constexpr const char* HTTP_414 = "HTTP/1.1 414 URI Too Long";
constexpr const char* HTTP_415 = "HTTP/1.1 415 Unsupported Media Type";
constexpr const char* HTTP_416 = "HTTP/1.1 416 Range Not Satisfiable";
constexpr const char* HTTP_417 = "HTTP/1.1 417 Expectation Failed";
constexpr const char* HTTP_418 = "HTTP/1.1 418 I'm a teapot";
constexpr const char* HTTP_421 = "HTTP/1.1 421 Misdirected Request";
constexpr const char* HTTP_422 = "HTTP/1.1 422 Unprocessable Entity";
constexpr const char* HTTP_423 = "HTTP/1.1 423 Locked";
constexpr const char* HTTP_424 = "HTTP/1.1 424 Failed Dependency";
constexpr const char* HTTP_425 = "HTTP/1.1 425 Too Early";
constexpr const char* HTTP_426 = "HTTP/1.1 426 Upgrade Required";
constexpr const char* HTTP_428 = "HTTP/1.1 428 Precondition Required";
constexpr const char* HTTP_429 = "HTTP/1.1 429 Too Many Requests";
constexpr const char* HTTP_431 = "HTTP/1.1 431 Request Header Fields Too Large";
constexpr const char* HTTP_451 = "HTTP/1.1 451 Unavailable For Legal Reasons";

// Server error responses
constexpr const char* HTTP_500 = "HTTP/1.1 500 Internal Server Error";
constexpr const char* HTTP_501 = "HTTP/1.1 501 Not Implemented";
constexpr const char* HTTP_502 = "HTTP/1.1 502 Bad Gateway";
constexpr const char* HTTP_503 = "HTTP/1.1 503 Service Unavailable";
constexpr const char* HTTP_504 = "HTTP/1.1 504 Gateway Timeout";
constexpr const char* HTTP_505 = "HTTP/1.1 505 HTTP Version Not Supported";
constexpr const char* HTTP_506 = "HTTP/1.1 506 Variant Also Negotiates";
constexpr const char* HTTP_507 = "HTTP/1.1 507 Insufficient Storage";
constexpr const char* HTTP_508 = "HTTP/1.1 508 Loop Detected";
constexpr const char* HTTP_510 = "HTTP/1.1 510 Not Extended";
constexpr const char* HTTP_511 = "HTTP/1.1 511 Network Authentication Required";

constexpr const ssize_t MIN_BUFFER_SIZE = 512;
constexpr const ssize_t MAX_HEADER_LENGTH = 8000;
constexpr const ssize_t MAX_HEADER_URL_LENGTH = 4000;

inline const char* acorn_header_rfc_validation(const std::string_view& view_http_buffer) {
    size_t headerBodySeparatorPos = view_http_buffer.find("\r\n\r\n");
    if (headerBodySeparatorPos == std::string_view::npos) {
        return HTTP_400;
    }

    if (headerBodySeparatorPos > MAX_HEADER_LENGTH) {
        return HTTP_431;
    }

    size_t startPos = 0;
    while (startPos < headerBodySeparatorPos) {
        size_t crlfPos = view_http_buffer.find("\r\n", startPos);
        if (crlfPos == std::string_view::npos || crlfPos > headerBodySeparatorPos) {
            return HTTP_400;
        }
        startPos = crlfPos + 2;
    }

    size_t firstLineEndPos = view_http_buffer.find("\r\n");
    if (firstLineEndPos == std::string_view::npos) {
        return HTTP_400; 
    }

    std::string_view firstLine = view_http_buffer.substr(0, firstLineEndPos);
    auto spaceCount = std::count(firstLine.begin(), firstLine.end(), ' ');
    if (spaceCount != 2) {
        return HTTP_400;
    }

    size_t firstSpacePos = firstLine.find(' ');
    size_t secondSpacePos = firstLine.rfind(' ');
    if (firstSpacePos == std::string_view::npos || secondSpacePos == std::string_view::npos || firstSpacePos == secondSpacePos) {
        return HTTP_400;
    }

    std::string_view method = firstLine.substr(0, firstSpacePos);
    std::string_view target = firstLine.substr(firstSpacePos + 1, secondSpacePos - firstSpacePos - 1);
    std::string_view httpVersion = firstLine.substr(secondSpacePos + 1);

    if (method != "GET" && method != "POST" && method != "HEAD") {
        return HTTP_405;
    }

    if (target.length() == 0 || target.length() > MAX_HEADER_URL_LENGTH) {
        return HTTP_414;
    }

    if (httpVersion != "HTTP/1.1" && httpVersion != "HTTP/1.0") {
        return HTTP_505;
    }

    return nullptr;
}

std::ostringstream acorn_header_parser(const std::string_view& view_http_buffer) {
    std::ostringstream responseStream;

     const char * responceCode = acorn_header_rfc_validation(view_http_buffer);
     if(responceCode != nullptr) {
        responseStream << responceCode << "\r\n";
        responseStream << "Server: Acorn HTTP Server\r\n";
        responseStream << "Content-Length: 0\r\n";
        responseStream << "\r\n";
    } else {

        std::unordered_map<std::string_view, std::string_view> header_map;
        
        size_t firstLineEndPos = view_http_buffer.find("\r\n");
        std::string_view firstLine = view_http_buffer.substr(0, firstLineEndPos);

        size_t firstSpacePos = firstLine.find(' ');
        size_t secondSpacePos = firstLine.rfind(' ');

        std::string_view method = firstLine.substr(0, firstSpacePos);
        std::string_view request_target = firstLine.substr(firstSpacePos + 1, secondSpacePos - firstSpacePos - 1);

        header_map["Method"] = method;
        header_map["Request-Target"] = request_target;

        std::time_t currentTime = std::time(nullptr);
        std::tm* currentTm = std::gmtime(&currentTime);
        char timeStr[80];
        std::strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S GMT", currentTm);
        std::string content = "<!DOCTYPE html><html lang=\"en\"><head><title>Acorn Web Server</title><meta charset=\"utf-8\"></head><body><h1>Hello, Welcome..</h1></body></html>";
        std::stringstream etagStream;
        etagStream << "\"" << std::hex << std::hash<std::string>{}(content) << "\"";
        std::string etag = etagStream.str();
        responseStream << HTTP_200 << "\r\n";
        responseStream << "Date: " << timeStr << "\r\n";
        responseStream << "Server: Acorn HTTP Server\r\n";
        responseStream << "Last-Modified: " << timeStr << "\r\n";
        responseStream << "ETag: " << etag << "\r\n";
        responseStream << "Accept-Ranges: bytes\r\n";
        responseStream << "Content-Length: " << content.length() << "\r\n";
        responseStream << "Vary: Accept-Encoding\r\n";
        responseStream << "Content-Type: text/html\r\n";
        responseStream << "\r\n";
        responseStream << content << "\r\n";
    }

    return responseStream;
}
#endif
