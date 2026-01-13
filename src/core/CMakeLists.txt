# 1. 设置 CMake 最低版本
cmake_minimum_required(VERSION 3.10)

# 2. 项目名称
project(HighPerformanceProxy)

# 3. 设置 C++ 标准 (C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 4. 自动查找当前目录下的所有 .cpp 和 .h 文件
# 将它们存入变量 SOURCE_FILES 中
#file(GLOB SOURCE_FILES "*.cpp" "*.h")

# 5. 生成可执行文件 my_server
add_executable(my_server main.cpp)

# 6. 链接必要的库
# pthread: 线程库 (必须)
# uring: io_uring 库 (如果系统没有安装，这行可能会报错，下面有解决方案)
target_link_libraries(my_server pthread)