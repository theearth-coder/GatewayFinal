#include "content_engine.h"
#include <string>

// API聚合：拼接多个后端响应为一个JSON
std::string ContentEngine::aggregateApiAll(const std::vector<std::string>& backend_responses) {
    std::string aggregated = "{\n";
    for (size_t i = 0; i < backend_responses.size(); ++i) {
        aggregated += "  \"backend_" + std::to_string(i+1) + "\": " + backend_responses[i];
        if (i != backend_responses.size() - 1) {
            aggregated += ",";
        }
        aggregated += "\n";
    }
    aggregated += "}";
    return aggregated;
}

// 强制插入X-Proxy-ID Header
void ContentEngine::forceInsertProxyHeader(HttpRequest& request, const std::string& team_name) {
    request.headers["x-proxy-id"] = team_name;  // 覆盖已有Header（如果存在）
}
