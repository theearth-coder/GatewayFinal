#!/bin/bash

# ====================================================
#  High Performance Server Startup Script
#  Role A: 深度优化 - 系统参数调优
# ====================================================

echo ">>> [System] 正在检查并优化 Linux 内核参数..."

# 1. 开启 HugePages (大页内存) 支持
# 对应 Module A - 内存管理优化 - 大页内存支持
# 作用：减少 TLB (Translation Lookaside Buffer) Miss，提升大内存访问性能
# 注意：修改 sysctl 需要 sudo 权限
echo ">>> [Optimization] Setting vm.nr_hugepages = 128..."
sudo sysctl -w vm.nr_hugepages=128

# 2. 验证是否生效
echo ">>> [Check] 当前 HugePages 状态："
grep Huge /proc/meminfo | head -n 4

echo "----------------------------------------------------"

# 3. 检查是否需要重新编译 (可选，为了方便)
if [ ! -f "./build/my_server" ]; then
    echo ">>> [Build] 未找到可执行文件，正在编译..."
    mkdir -p build
    cd build
    cmake ..
    make
    cd ..
fi

# 4. 启动服务器
echo ">>> [Run] 启动高性能服务器..."
./build/my_server