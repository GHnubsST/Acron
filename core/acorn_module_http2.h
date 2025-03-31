#ifndef ACORN_MODULE_HTTP2_H
#define ACORN_MODULE_HTTP2_H

#include <unordered_map>
#include <fstream>
#include <sstream>
#include <ctime>
#include <openssl/ssl.h>
#include <openssl/err.h>

constexpr const char *HTTP2_SETTINGS = "\x00\x00\x00\x04\x00\x00\x00\x00\x00"; // Empty SETTINGS frame
constexpr const char *HTTP2_MAGIC = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";        // Connection preface

struct Http2Frame
{
    uint32_t length;
    uint8_t type;
    uint8_t flags;
    uint32_t streamId;
    std::string payload;
};

std::unordered_map<std::string, std::string> mimeTypes = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".gif", "image/gif"},
    {".txt", "text/plain"}};

std::string getMimeType(const std::string &path)
{
    size_t dot = path.rfind('.');
    if (dot != std::string::npos)
    {
        auto it = mimeTypes.find(path.substr(dot));
        if (it != mimeTypes.end())
            return it->second;
    }
    return "application/octet-stream";
}

class acorn_http2
{
public:
    static bool init_ssl(SSL_CTX *&ctx)
    {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();

        ctx = SSL_CTX_new(TLS_server_method());
        if (!ctx)
            return false;

        SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
        SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
        SSL_CTX_set_alpn_protos(ctx, (const unsigned char *)"\x02h2", 3); // Advertise HTTP/2

        if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0 ||
            SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0)
        {
            SSL_CTX_free(ctx);
            return false;
        }
        return true;
    }

    static std::string serve_file(const std::string &target)
    {
        std::string filePath = "html" + (target == "/" ? "/index.html" : target);
        if (filePath.find("..") != std::string::npos)
            filePath = "html/index.html";

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
            return "";

        std::ostringstream contentStream;
        contentStream << file.rdbuf();
        return contentStream.str();
    }

    static std::string build_headers_frame(uint32_t streamId, const std::string &content, const std::string &path)
    {
        std::string headers;
        std::string mime = getMimeType(path);
        std::time_t now = std::time(nullptr);
        char date[80];
        std::strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&now));

        // Simplified HPACK-like headers (not fully compressed for simplicity)
        headers += "\x83"; // :status: 200 (pre-encoded in HPACK table)
        headers += "\x40\x0a"
                   "content-type"
                   "\x00" +
                   char(mime.length()) + mime;
        headers += "\x40\x04"
                   "date"
                   "\x00" +
                   char(strlen(date)) + date;
        headers += "\x40\x0d"
                   "content-length"
                   "\x00" +
                   char(std::to_string(content.length()).length()) + std::to_string(content.length());

        uint32_t length = headers.length();
        std::string frame = std::string(3, 0) + "\x01" + "\x05" + std::string(4, 0); // HEADERS frame, END_HEADERS flag
        frame[0] = (length >> 16) & 0xFF;
        frame[1] = (length >> 8) & 0xFF;
        frame[2] = length & 0xFF;
        frame[3 + 1] = streamId & 0x7F; // Stream ID (31 bits, no reserved bit)
        frame[3 + 2] = (streamId >> 8) & 0xFF;
        frame[3 + 3] = (streamId >> 16) & 0xFF;
        frame[3 + 4] = (streamId >> 24) & 0xFF;
        return frame + headers;
    }

    static std::string build_data_frame(uint32_t streamId, const std::string &content)
    {
        uint32_t length = content.length();
        std::string frame = std::string(3, 0) + "\x00" + "\x01" + std::string(4, 0); // DATA frame, END_STREAM flag
        frame[0] = (length >> 16) & 0xFF;
        frame[1] = (length >> 8) & 0xFF;
        frame[2] = length & 0xFF;
        frame[3 + 1] = streamId & 0x7F;
        frame[3 + 2] = (streamId >> 8) & 0xFF;
        frame[3 + 3] = (streamId >> 16) & 0xFF;
        frame[3 + 4] = (streamId >> 24) & 0xFF;
        return frame + content;
    }
};

#endif