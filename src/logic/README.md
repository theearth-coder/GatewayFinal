# TCP 代理服务器 - Linux 实验（角色B）
## 角色B负责模块
1. 多协议解析（HTTP/1.1、HTTP/2降级、WebSocket透传）
2. 后端服务发现与负载均衡（轮询、最少连接、GPU感知）
3. 协议转换（HTTP→gRPC）、内容处理（API聚合）

## 快速编译
```bash
mkdir build && cd build
cmake .. && make
./proxy_server  # 运行EchoServer
