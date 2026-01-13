cmake_minimum_required(VERSION 3.10)
project(GatewayFinal)

set(CMAKE_CXX_STANDARD 17)

include_directories(
    src
    src/core
    src/logic/src/logic
    src/logic/src/common
    src/common
)

file(GLOB_RECURSE SOURCES "src/*.cpp")

# 排除干扰文件
list(FILTER SOURCES EXCLUDE REGEX "src/core/main.cpp")
list(FILTER SOURCES EXCLUDE REGEX "CompilerIdCXX")

# 生成程序并链接库
add_executable(my_gateway ${SOURCES})
target_link_libraries(my_gateway pthread dl z)
