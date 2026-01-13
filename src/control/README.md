# 高性能 TCP/UDP 代理服务器

> 约定：**控制面目录固定为 `control/`**；代理默认配置文件名为 **`proxy_config.json`**（由控制面生成/更新）。

---

## 0. 角色分工与整合目标

### 角色 A/B（C++ 代理核心）负责（数据面）
- 启动 TCP/UDP 代理服务，接收客户端连接与转发。
- 实现负载均衡策略（RR / LeastConn / Weighted 等）与连接管理。
- **必须支持热重载**：收到 `SIGHUP` 后重新加载配置并更新后端池。
- **必须写 PID 文件**：将本进程 PID 写到 `control/run/proxy.pid`，供控制面发信号。

### 角色 C负责（控制面）
- 提供 Web 控制台：展示后端节点、在线改权重、诊断工具（Ping）、审计日志 tail。
- 服务发现模拟（k8s_endpoints.json）+ 动态注册（/api/register）+ 合并生成最终配置。
- 写入 `control/config/proxy_config.json` 并向代理发 `SIGHUP` 实现热重载闭环。

> ✅ 整合成功的标志：  
> **网页修改 weight / 服务发现变化 / 动态注册上报** → `proxy_config.json` 更新 → 代理收到 `SIGHUP` 热重载 → 转发策略立刻生效（无需重启）。

---

## 1. 仓库目录结构

> 控制面放到 `control/`，A/B 代理可放 `proxy/`（名字可改，但建议保持清晰）。

---

## 2. A/B 必做的“整合契约”（最关键）

### 2.1 配置文件路径与默认名（必须）
- 配置文件默认名：`proxy_config.json`
- **强烈建议代理默认读取：**
  - `control/config/proxy_config.json`
- 代理也可以支持参数：
  - `--config control/config/proxy_config.json`
- 代理收到 SIGHUP 时必须 **重新读取该配置文件** 并更新后端池。

### 2.2 PID 文件路径（必须）
代理启动后立刻写入：

- `control/run/proxy.pid`

内容只写一个整数 PID（例如 `12345`）。

### 2.3 SIGHUP 热重载（必须）
代理进程注册信号处理：

- `SIGHUP` → `reload_config()` → 更新后端列表/权重/enabled/is_warming_up 等

---

## 3. 配置文件格式（proxy_config.json 规范）

控制面写出的 `control/config/proxy_config.json` 格式如下（示例）：

```json
{
  "listen": { "host": "0.0.0.0", "port": 10080 },
  "algorithm": "weighted_round_robin",
  "updated_at": 1730000000,
  "backends": [
    {
      "ip": "10.0.0.11",
      "port": 9001,
      "weight": 10,
      "enabled": true,
      "is_warming_up": false,
      "gpu_usage": 0.2,
      "vram_usage": 0.4
    }
  ]
}

字段说明：
listen.host/port：代理监听地址与端口（A/B 用于 bind/listen）
algorithm：调度算法名称（A/B 用于选择策略）
backends[]：后端池（A/B 必须支持至少 ip/port/weight/enabled）
is_warming_up：预热节点（A/B 可以将权重视为 0 或降低）
gpu_usage/vram_usage：GPU 感知调度可用指标（B 可选用）
```

## 4. 控制面（control/）各文件作用说明
4.1 control/app.py（核心）
Flask 服务入口（默认 0.0.0.0:8080）
主要功能：
/：渲染控制台页面
/api/backends：返回合并后的后端列表（k8s + runtime）
/api/register：动态注册后端（写入 data/backends_runtime.json）
/api/backends/<ip>/<port>/weight：在线改 weight（写 runtime，并触发同步）
/api/sync：手动触发同步（写 proxy_config.json + SIGHUP）
/api/ping：Ping 工具
/api/logs/audit：读取 logs/audit.log 末尾 N 行
Step3：后台线程监听 k8s_endpoints.json/backends_runtime.json 变更，自动同步写配置并发 SIGHUP。

4.2 control/config/control_config.json
控制面自身配置（端口、路径、TTL 等）
你可以在这里改默认监听端口、autosync 间隔等。

4.3 control/config/proxy_config.json
最终给代理读的配置文件（控制面生成）
每次服务发现变化/注册/改权重都会更新它。

4.4 control/data/k8s_endpoints.json
服务发现模拟输出（脚本生成）
控制面会读它，合并进后端池（source=k8s 的概念在 UI 体现）。

4.5 control/data/backends_runtime.json
动态注册与覆盖层（runtime）
/api/register、改 weight 都写这里
该表覆盖 k8s 表中同 ip:port 的信息（优先级更高）。

4.6 control/run/proxy.pid
代理进程 PID 文件
控制面通过它 kill(pid, SIGHUP) 通知代理热重载。

4.7 control/logs/audit.log
控制面审计日志（记录访问、诊断等，供 Step4 “日志查看器”演示）

4.8 control/templates/index.html
控制台页面（你的深色风格）
包含：后端列表表格 Ping 弹窗 日志输出区

4.9 control/static/main.js
控制台前端逻辑：
刷新后端列表、过滤、保存 weight
打开 Ping 弹窗并调用 /api/ping
加载 audit.log tail 并显示到 logBox
手动同步按钮调用 /api/sync

4.10 control/static/style.css
控制台 UI 风格（当前深色主题）

4.11 control/scripts/gen_k8s_endpoints.py
服务发现模拟脚本：
--loop 时周期更新 data/k8s_endpoints.json
模拟节点上下线、GPU/VRAM 变化

4.12 control/scripts/sim_backend_register_batch.py
Step4：批处理 mock 演示脚本
--mode nobatch：5 个小请求显存按 5 份增长（更快）
--mode batch：5 个小请求合成一批显存只加 1 份（更省）

4.13 control/scripts/dummy_proxy.py（可选）
没有真 C++ 代理时，用它写 PID 并打印 SIGHUP 收到情况
用于验证“控制面 → SIGHUP → 热重载触发链路”已打通。

## 5. 如何运行（虚拟机 Ubuntu + VSCode）
5.1 启动控制面（角色C）
cd ~/桌面/control
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python app.py

浏览器打开：http://127.0.0.1:8080

5.2 启动代理（角色A/B）
代理需要读配置 control/config/proxy_config.json 并写 PID 到 control/run/proxy.pid
示例（建议 A/B 支持该参数）：
cd repo_root
./proxy/bin/proxy_server --config control/config/proxy_config.json

代理启动后你必须验证：cat control/run/proxy.pid 能看到 PID 数字。

5.3 启动服务发现模拟（可选但强烈建议，用来演示 Step3）
cd ~/桌面/control
source .venv/bin/activate
python scripts/gen_k8s_endpoints.py --loop --interval 5

5.4 启动批处理 mock（Step4 演示核心）
先跑不批处理（显存更快涨）：
cd ~/桌面/control
source .venv/bin/activate
python scripts/sim_backend_register_batch.py --mode nobatch

然后 Ctrl+C，切换到批处理（显存更省）：
python scripts/sim_backend_register_batch.py --mode batch

如果 A/B 代理未就绪，也可以先跑：python scripts/dummy_proxy.py 以验证 SIGHUP 热重载链路。

## 6. 网页怎么演示

建议开 3~4 个终端：control / proxy / k8s模拟 / batch模拟。
网页地址：http://127.0.0.1:8080

演示 1：后端列表随服务发现变化（Step3）
终端在跑 gen_k8s_endpoints.py --loop
网页点 “刷新”
过 5 秒再点一次 “刷新”
说明：k8s_endpoints.json 更新 → control 自动合并生成 proxy_config.json → 尝试 SIGHUP 通知代理热重载

演示 2：动态注册上报（Step3）
终端跑 sim_backend_register_batch.py（它会周期 POST /api/register）
网页点 “刷新”
看到 source=runtime 的节点出现/指标变化（GPU/VRAM）

演示 3：在线改权重 + 热重载（整合闭环核心）
在表格里修改某行 weight（例如 10 改 30）
点击该行 “保存”
页面状态区会显示 /api/backends/<ip>/<port>/weight 的返回，其中包含 reload 信息：
sent SIGHUP to pid=xxxx（成功）
同时 A/B 代理终端应打印 “reload config success” 或类似日志
这一步是 A/B/C 整合最重要的“验收点”：无需重启，配置立刻生效。

演示 4：Ping 工具（Step4）
点击 “Ping 工具”
输入某个后端 IP（或 127.0.0.1）
点击 “开始 Ping”
弹窗与日志输出区显示 ping 结果

演示 5：审计日志 tail（Step4）
点击右上角 “查看审计日志”（或日志卡片内“加载 audit.log”）
日志输出区展示 audit.log 末尾 N 行（默认 100）
说明：用于追踪访问、诊断、策略调整等操作记录

演示 6：批处理 mock（Step4 亮点）
终端先跑 --mode nobatch
网页刷新观察 vram_usage：上升更快、更高
切换终端为 --mode batch
再刷新观察 vram_usage：更平滑/更低
说明：同样 5 个小请求，批处理合并为一批，显存只增加 1 份（mock 演示）