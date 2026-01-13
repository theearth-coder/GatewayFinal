#include "load_balancer.h"
#include <algorithm>
#include <mutex>
#include <cstdint>

std::mutex g_backend_mutex;  // 保护后端列表的线程安全（多线程环境下必加）

// 辅助函数：过滤预热中的节点，返回可用节点列表
std::vector<BackendServer*> getAvailableBackends(std::vector<BackendServer>& backends) {
    std::vector<BackendServer*> available;
    for (auto& backend : backends) {
        if (backend.checkWarmupFinish()) {  // 调用之前实现的预热检查
            available.push_back(&backend);
        }
    }
    return available;
}

// 添加后端节点
void LoadBalancer::addBackend(const BackendServer& backend) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    backends.emplace_back(backend);
    conn_counts[&backends.back()] = 0;  // 初始连接数为0
}

// 轮询算法：依次选择可用节点
BackendServer* LoadBalancer::selectRoundRobin() {
    auto available = getAvailableBackends(backends);
    if (available.empty()) return nullptr;

    // 原子自增索引，取模得到当前选择的节点（线程安全）
    uint32_t idx = rr_index.fetch_add(1, std::memory_order_relaxed) % available.size();
    return available[idx];
}

// 最少连接数算法：选择当前连接数最少的节点
BackendServer* LoadBalancer::selectLeastConn() {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    auto available = getAvailableBackends(backends);
    if (available.empty()) return nullptr;

    BackendServer* selected = nullptr;
    uint32_t min_conn = UINT32_MAX;
    // 遍历所有可用节点，找连接数最少的
    for (auto* backend : available) {
        uint32_t conn = conn_counts[backend].load();
        if (conn < min_conn) {
            min_conn = conn;
            selected = backend;
        }
    }
    return selected;
}

// GPU感知算法：显存越空闲，权重越高（weight = (1 - vram_usage) * 初始权重）
BackendServer* LoadBalancer::selectGPUAware() {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    auto available = getAvailableBackends(backends);
    if (available.empty()) return nullptr;

    BackendServer* selected = nullptr;
    float max_weight = -1.0f;
    // 遍历所有可用节点，找GPU权重最高的
    for (auto* backend : available) {
        float weight = backend->getGPUAwareWeight();  // 调用结构体的权重计算函数
        if (weight > max_weight) {
            max_weight = weight;
            selected = backend;
        }
    }
    return selected;
}

// 核心选择接口：根据配置的算法类型选择节点
BackendServer* LoadBalancer::selectBackend() {
    switch (lb_type) {
        case ROUND_ROBIN: return selectRoundRobin();
        case LEAST_CONN: return selectLeastConn();
        case GPU_AWARE: return selectGPUAware();
        default: return selectRoundRobin();  // 默认轮询
    }
}

// 会话保持：源地址哈希（Hash(ClientIP) % 可用节点数）
BackendServer* LoadBalancer::selectByClientIP(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    auto available = getAvailableBackends(backends);
    if (available.empty()) return nullptr;

    // IP哈希函数（简单实现：将IP字符串转为整数）
    auto ip_hash = [](const std::string& ip) -> uint32_t {
        uint32_t hash = 0;
        for (char c : ip) {
            if (c != '.') hash = hash * 10 + (c - '0');
        }
        return hash;
    };

    uint32_t hash_val = ip_hash(client_ip);
    uint32_t idx = hash_val % available.size();
    return available[idx];
}

// 增加连接数（连接建立时调用）
void LoadBalancer::incrConnCount(BackendServer* backend) {
    if (backend) {
        conn_counts[backend].fetch_add(1, std::memory_order_relaxed);
    }
}

// 减少连接数（连接关闭时调用）
void LoadBalancer::decrConnCount(BackendServer* backend) {
    if (backend) {
        conn_counts[backend].fetch_sub(1, std::memory_order_relaxed);
    }
}
