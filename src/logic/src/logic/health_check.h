#ifndef HEALTH_CHECK_H
#define HEALTH_CHECK_H

#include <string>

class HealthCheck {
public:
    // 预留Shell脚本接口：模拟执行外部脚本进行健康检查，总是返回True
    bool run_script(const char* script_path);
};

#endif // HEALTH_CHECK_H
