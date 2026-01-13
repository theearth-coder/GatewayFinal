#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <string>
#include <unordered_map>

// HTTP请求结构体
struct HttpRequest {
    std::string method;          // GET/POST
    std::string path;            // 请求路径
    std::string version;         // HTTP/1.1
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    // 协议类型（降级/透传）
    enum ProtocolType {
        HTTP_1_1,
        HTTP_2_DOWNGrade,
        WEBSOCKET
    } protocol_type;
};

// HTTP解析器类
class HttpParser {
public:
    bool parse(const std::string& raw_data, HttpRequest& request);
private:
    bool parseRequestLine(const std::string& line, HttpRequest& request);
    bool parseHeaderLine(const std::string& line, HttpRequest& request);
    void detectProtocol(HttpRequest& request);
};

#endif // HTTP_PARSER_H
