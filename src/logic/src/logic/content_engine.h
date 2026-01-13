#ifndef CONTENT_ENGINE_H
#define CONTENT_ENGINE_H

#include <string>
#include <vector>
#include "http_parser.h"

class ContentEngine {
public:
    // 1. API聚合（Mock：请求/api/all时，拼接两个后端的响应）
    std::string aggregateApiAll(const std::vector<std::string>& backend_responses);

    // 2. 强制插入Header：X-Proxy-ID: TeamName（替换为你的团队名）
    void forceInsertProxyHeader(HttpRequest& request, const std::string& team_name = "TeamB-LinuxExp");
};

#endif // CONTENT_ENGINE_H
