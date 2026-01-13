#include "http_parser.h"
#include <sstream>
#include <algorithm>

// 字符串转小写
std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

// 解析请求行
bool HttpParser::parseRequestLine(const std::string& line, HttpRequest& request) {
    std::istringstream iss(line);
    iss >> request.method >> request.path >> request.version;
    return !request.method.empty() && !request.path.empty() && !request.version.empty();
}

// 解析请求头
bool HttpParser::parseHeaderLine(const std::string& line, HttpRequest& request) {
    auto colon_pos = line.find(':');
    if (colon_pos == std::string::npos) return false;
    std::string key = toLower(line.substr(0, colon_pos));
    std::string value = line.substr(colon_pos + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);
    request.headers[key] = value;
    return true;
}

// 检测协议类型
void HttpParser::detectProtocol(HttpRequest& request) {
    // 检测WebSocket
    auto upgrade_it = request.headers.find("upgrade");
    if (upgrade_it != request.headers.end() && toLower(upgrade_it->second) == "websocket") {
        request.protocol_type = HttpRequest::WEBSOCKET;
        return;
    }
    // 检测HTTP/2降级
    if (request.version == "HTTP/2.0") {
        request.protocol_type = HttpRequest::HTTP_2_DOWNGrade;
        request.version = "HTTP/1.1";
        return;
    }
    // 默认HTTP/1.1
    request.protocol_type = HttpRequest::HTTP_1_1;
}

// 核心解析接口
bool HttpParser::parse(const std::string& raw_data, HttpRequest& request) {
    std::istringstream iss(raw_data);
    std::string line;

    // 解析请求行
    if (!std::getline(iss, line) || !parseRequestLine(line, request)) {
        return false;
    }

    // 解析请求头
    while (std::getline(iss, line) && line != "\r" && line != "") {
        parseHeaderLine(line, request);
    }

    // 解析请求体
    std::string body;
    while (std::getline(iss, line)) {
        body += line + "\n";
    }
    request.body = body;

    // 检测协议
    detectProtocol(request);
    return true;
}
