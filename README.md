# AI Gateway: 高性能 GPU 感知智能网关系统

本项目是一个专为 AI 推理场景设计的高性能网关系统。针对大模型推理任务对显存资源的依赖，实现了基于 C++ Epoll 的高性能转发引擎与基于 Python 的可视化管控平面，支持 GPU 状态感知与动态流量调度。

## 1. 项目核心功能与实验要求对应

本项目严格遵循实验技术规范，实现了以下模块：

* **模块 A/C: 高性能核心引擎 (High-Performance Core)**
    * 基于 **Linux Epoll** 事件驱动模型，采用原生 Socket API 开发，支持高并发非阻塞 I/O。
    * 实现 HTTP 协议的基础解析与响应，保证低延迟转发。

* **模块 B: GPU 感知负载均衡 (GPU-Aware Load Balancing)**
    * 实现了基于 **加权轮询 (Weighted Round-Robin)** 的调度算法。
    * **动态感知**: 网关实时读取后端节点的 GPU 显存 (VRAM) 和利用率 (Usage) 数据，自动调整分发策略。

* **模块 D: 监控与管理系统 (Control Plane)**
    * **可视化控制台**: 基于 Python Flask 开发，提供 Web 界面查看节点健康状态与审计日志。
    * **配置热重载**: 支持通过 Web 界面动态调整权重或下线节点，底层 C++ 引擎通过 **SIGHUP 信号** 无缝加载新配置，实现零宕机运维。

## 2. 项目结构说明

* `src/main_final.cpp`: C++ 核心网关源代码 (数据面)。
* `src/control/app.py`: Python 管理后台源代码 (控制面)。
* `src/control/data/k8s_endpoints.json`: 共享配置存储，模拟服务发现机制。
* `CMakeLists.txt`: 项目构建文件。

## 3. 快速启动指南

### 环境依赖
* Ubuntu 20.04+ (虚拟机)
* G++ / CMake
* Python 3

### 第一步：编译核心引擎
```bash
cd ~/GatewayFinal
mkdir -p build && cd build
cmake ..
make
# 编译成功后会生成 gateway_proxy 可执行文件
```

### 第二步：启动管理后台 (控制面)
打开 【终端 1】，启动 Python 控制台：
```
cd ~/GatewayFinal/src/control
python3 app.py
# 服务将启动在 http://127.0.0.1:8080
```

### 第三步：启动网关引擎 (数据面)
打开 【终端 2】，启动 C++ 网关：
```
cd ~/GatewayFinal/build
./gateway_proxy
# 网关将监听 8000 端口 (视配置而定)，并开始监听配置文件变化
```

### 第四步:验证高性能转发
* **场景一：验证高性能转发**
* 目的: 验证核心引擎对 HTTP 请求的处理能力。

    在 【终端 3】 中发送请求：
  ```
  curl -v http://127.0.0.1:8000
  ```
* **场景二：展示可视化管控与配置热重载**
* 打开控制台: 浏览器访问 http://127.0.0.1:8080。
