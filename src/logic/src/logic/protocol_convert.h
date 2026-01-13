#ifndef PROTOCOL_CONVERT_H
#define PROTOCOL_CONVERT_H

#include <string>
#include "http_parser.h"  // 依赖HTTP请求结构体

class ProtocolConverter {
public:
    // 1. HTTP → gRPC（Mock：拼接伪造的gRPC Frame格式）
    std::string httpToGrpcMock(const HttpRequest& request);

    // 2. JSON → Protobuf（Mock：简单字符串替换，演示格式转换）
    std::string jsonToProtobufMock(const std::string& json_str);

    // 3. 压缩/解压（调用zlib库，真实实现）
    std::string compressData(const std::string& data);  // 压缩（gzip格式）
    std::string decompressData(const std::string& compressed_data);  // 解压
};

#endif // PROTOCOL_CONVERT_H
