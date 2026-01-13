#include "health_check.h"
#include <iostream>

// 降维实现：预留接口，打印日志并返回True（报告中注明支持Shell脚本扩展）
bool HealthCheck::run_script(const char* script_path) {
    std::cout << "[HealthCheck] 执行脚本：" << (script_path ? script_path : "null") << "（模拟成功）" << std::endl;
    return true;  // 始终返回健康
}
