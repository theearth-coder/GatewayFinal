#include "protocol_convert.h"
#include <zlib.h>
#include <stdexcept>
#include <algorithm>
#include <cstring>         
#include <arpa/inet.h>

// HTTP→gRPC Mock：gRPC Frame格式为「4字节长度 + 1字节标志 + 数据」
std::string ProtocolConverter::httpToGrpcMock(const HttpRequest& request) {
    std::string grpc_data = "/" + request.path + ":" + request.body;  // 模拟gRPC方法和数据

    // 构建gRPC Frame
    uint32_t length = htonl(grpc_data.size());  // 长度字段（大端序）
    uint8_t flag = 0x00;  // 0x00表示非压缩
    std::string frame;
    frame.append(reinterpret_cast<char*>(&length), 4);  // 长度
    frame.append(reinterpret_cast<char*>(&flag), 1);    // 标志
    frame.append(grpc_data);                             // 数据
    return frame;
}

// JSON→Protobuf Mock：替换{}为<>、:为=，演示格式转换能力
std::string ProtocolConverter::jsonToProtobufMock(const std::string& json_str) {
    std::string proto_str = json_str;
    std::replace(proto_str.begin(), proto_str.end(), '{', '<');
    std::replace(proto_str.begin(), proto_str.end(), '}', '>');
    std::replace(proto_str.begin(), proto_str.end(), ':', '=');
    return proto_str;
}

// 压缩数据（zlib，gzip格式）
std::string ProtocolConverter::compressData(const std::string& data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw std::runtime_error("zlib compress init failed");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = data.size();

    std::string compressed;
    char buf[4096];
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof(buf);
        if (deflate(&zs, Z_FINISH) == Z_STREAM_ERROR) {
            deflateEnd(&zs);
            throw std::runtime_error("zlib compress failed");
        }
        compressed.append(buf, sizeof(buf) - zs.avail_out);
    } while (zs.avail_out == 0);

    deflateEnd(&zs);
    return compressed;
}

// 解压数据（zlib，gzip格式）
std::string ProtocolConverter::decompressData(const std::string& compressed_data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) {
        throw std::runtime_error("zlib decompress init failed");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed_data.data()));
    zs.avail_in = compressed_data.size();

    std::string decompressed;
    char buf[4096];
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof(buf);
        if (inflate(&zs, Z_FINISH) == Z_STREAM_ERROR) {
            inflateEnd(&zs);
            throw std::runtime_error("zlib decompress failed");
        }
        decompressed.append(buf, sizeof(buf) - zs.avail_out);
    } while (zs.avail_out == 0);

    inflateEnd(&zs);
    return decompressed;
}
