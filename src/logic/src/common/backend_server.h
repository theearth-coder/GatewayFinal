#ifndef BACKEND_SERVER_H
#define BACKEND_SERVER_H

#include <string>
#include <chrono>

// 后端服务器结构体（支持GPU感知调度和预热）
struct BackendServer {
    std::string ip;                // 后端IP
    uint16_t port;                 // 后端端口
    uint32_t weight;               // 初始权重
    float gpu_usage;               // GPU使用率（0.0~1.0）
    float vram_usage;              // 显存使用率（0.0~1.0）
    bool is_warming_up;            // 预热标志位
    std::chrono::steady_clock::time_point warmup_start_time;

    // 构造函数
    BackendServer(std::string ip_, uint16_t port_, uint32_t weight_ = 1)
        : ip(ip_), port(port_), weight(weight_), gpu_usage(0.0f), vram_usage(0.0f),
          is_warming_up(true), warmup_start_time(std::chrono::steady_clock::now()) {}

    // 检查预热是否完成（5秒后恢复权重）
    bool checkWarmupFinish() {
        if (!is_warming_up) return true;
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - warmup_start_time
        ).count();
        if (duration >= 5) {
            is_warming_up = false;
            weight = 1;
            return true;
        }
        return false;
    }

    // GPU感知权重（显存越空闲，权重越高）
    float getGPUAwareWeight() {
        return is_warming_up ? 0.0f : (1.0f - vram_usage) * weight;
    }
};

#endif // BACKEND_SERVER_H
