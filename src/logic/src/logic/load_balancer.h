#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

#include "backend_server.h"  // 依赖之前定义的BackendServer结构体
#include <vector>
#include <atomic>
#include <unordered_map>
#include <string>

// 负载均衡算法类型
enum LoadBalanceType {
    ROUND_ROBIN,    // 轮询
    LEAST_CONN,     // 最少连接数
    GPU_AWARE       // GPU感知（显存使用率）
};

class LoadBalancer {
public:
    LoadBalancer(LoadBalanceType type = ROUND_ROBIN) : lb_type(type), rr_index(0) {}

    // 添加后端节点（从k8s_endpoints.json读取后调用此接口）
    void addBackend(const BackendServer& backend);

    // 核心接口：选择后端节点
    BackendServer* selectBackend();

    // 会话保持接口：按客户端IP选择后端（同一IP始终路由到同一节点）
    BackendServer* selectByClientIP(const std::string& client_ip);

    // 连接数管理（最少连接数算法用）
    void incrConnCount(BackendServer* backend);  // 连接建立时调用
    void decrConnCount(BackendServer* backend);  // 连接关闭时调用

private:
    LoadBalanceType lb_type;
    std::vector<BackendServer> backends;  // 后端节点列表
    std::atomic<uint32_t> rr_index;      // 轮询索引（原子变量保证线程安全）
    // 记录每个后端的当前连接数（最少连接数算法核心）
    std::unordered_map<BackendServer*, std::atomic<uint32_t>> conn_counts;

    // 私有实现：轮询算法
    BackendServer* selectRoundRobin();
    // 私有实现：最少连接数算法
    BackendServer* selectLeastConn();
    // 私有实现：GPU感知算法
    BackendServer* selectGPUAware();
};

#endif // LOAD_BALANCER_H
