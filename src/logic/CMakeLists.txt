cmake_minimum_required(VERSION 3.10)
project(TCPProxy)

# 设置 C++17 标准（兼容所有新代码）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引入 zlib 库（协议转换的压缩/解压需要）
find_package(ZLIB REQUIRED)

# 公共头文件路径（让所有代码能找到 src/common/ 下的头文件）
include_directories(src/common)
# logic 目录头文件路径（让主程序能找到 HTTP 解析器、负载均衡的头文件）
include_directories(src/logic)

# 1. 编译 echo_server（基础 TCP 服务器，保留）
add_executable(echo_server src/echo_server.cpp)

# 2. 编译逻辑库（角色B核心代码：HTTP解析+负载均衡+后续扩展）
add_library(logic_lib
    src/logic/http_parser.cpp
    src/logic/load_balancer.cpp   
    src/logic/protocol_convert.cpp
    src/logic/content_engine.cpp
    src/logic/health_check.cpp
)
# 链接 zlib 到逻辑库（压缩/解压需要）
target_link_libraries(logic_lib ZLIB::ZLIB)

# 3. 编译代理主程序（核心演示程序：整合所有功能）
add_executable(proxy_server src/proxy_main.cpp)
# 链接逻辑库到主程序（依赖 HTTP 解析器、负载均衡等）
target_link_libraries(proxy_server logic_lib)
