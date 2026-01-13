#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
控制面（角色C）：Flask 控制台 + 服务发现模拟 + 动态注册 + 配置管理 + 热重载触发（SIGHUP）
Step3 已集成：
- 监控 k8s_endpoints.json / backends_runtime.json 文件变化 -> 自动生成 proxy_config.json -> 自动 SIGHUP
- /api/register 动态注册写 runtime 表后，立即同步 + SIGHUP
- runtime TTL：多久没上报 last_seen 就自动 enabled=false（演示下线）
- 手动同步接口 /api/sync（调试/演示用）
"""

import json
import os
import signal
import subprocess
import threading
import time
from threading import Lock
from typing import Any, Dict, List, Tuple

from flask import Flask, jsonify, render_template, request

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


# ------------------------- 文件与配置工具 -------------------------

def abspath(p: str) -> str:
    """把 control/ 下的相对路径转成绝对路径。"""
    if os.path.isabs(p):
        return p
    return os.path.abspath(os.path.join(BASE_DIR, p))


def read_json(path: str, default: Any):
    """读取 JSON，失败则返回 default。"""
    path = abspath(path)
    if not os.path.exists(path):
        return default
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return default


def write_json_atomic(path: str, obj: Any):
    """原子写：先写 tmp 再 replace，避免写一半导致代理读到坏文件。"""
    path = abspath(path)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)
    os.replace(tmp, path)


def tail_file(path: str, n: int = 100) -> str:
    """简单 tail：读最后 n 行（日志不大时够用）。"""
    path = abspath(path)
    if not os.path.exists(path):
        return f"[control] file not found: {path}"
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()
    return "".join(lines[-n:])


def load_control_config() -> Dict[str, Any]:
    """读取控制面配置，并补齐默认值。"""
    cfg = read_json("config/control_config.json", {})

    cfg.setdefault("host", "0.0.0.0")
    cfg.setdefault("port", 8080)

    cfg.setdefault("proxy_pid_path", "../run/proxy.pid")
    cfg.setdefault("audit_log_path", "../logs/audit.log")

    cfg.setdefault("k8s_endpoints_path", "data/k8s_endpoints.json")
    cfg.setdefault("runtime_backends_path", "data/backends_runtime.json")
    cfg.setdefault("proxy_config_path", "config/proxy_config.json")

    cfg.setdefault("default_listen_host", "0.0.0.0")
    cfg.setdefault("default_listen_port", 10080)
    cfg.setdefault("default_algorithm", "weighted_round_robin")

    # Step3：自动同步与 TTL
    cfg.setdefault("auto_sync_interval_sec", 2)   # 文件变化轮询间隔
    cfg.setdefault("runtime_ttl_sec", 15)         # runtime 多久没上报就自动禁用（<=0 表示关闭）

    return cfg


CFG = load_control_config()
app = Flask(__name__, template_folder=abspath("templates"), static_folder=abspath("static"))


# ------------------------- 与代理进程交互（SIGHUP） -------------------------

def try_reload_proxy() -> Tuple[bool, str]:
    """
    读取 pid 文件并发送 SIGHUP。
    如果 pid 文件不存在/权限不足/进程不存在，会返回失败信息（控制面仍可工作）。
    """
    pid_path = abspath(CFG["proxy_pid_path"])
    if not os.path.exists(pid_path):
        return False, f"pid file not found: {pid_path}"

    try:
        with open(pid_path, "r", encoding="utf-8") as f:
            s = f.read().strip()
        pid = int(s)
        os.kill(pid, signal.SIGHUP)
        return True, f"sent SIGHUP to pid={pid}"
    except ProcessLookupError:
        return False, f"process not found for pid file: {pid_path}"
    except PermissionError:
        return False, f"permission denied sending SIGHUP: {pid_path}"
    except Exception as e:
        return False, f"reload error: {e}"


# ------------------------- 后端数据结构与合并 -------------------------

def normalize_backend(b: Dict[str, Any], source: str) -> Dict[str, Any]:
    """把 k8s/runtime 不同来源的数据统一成一个结构。"""
    return {
        "ip": str(b.get("ip", "")).strip(),
        "port": int(b.get("port", 0) or 0),
        "weight": int(b.get("weight", 0) or 0),
        "gpu_usage": b.get("gpu_usage", None),
        "vram_usage": b.get("vram_usage", None),
        "enabled": bool(b.get("enabled", True)),
        "is_warming_up": bool(b.get("is_warming_up", False)),
        "last_seen": int(b.get("last_seen", 0) or 0),
        "source": source,
    }


def key_of(b: Dict[str, Any]) -> str:
    return f"{b.get('ip', '')}:{int(b.get('port', 0) or 0)}"


def merge_backends(k8s: List[Dict[str, Any]], runtime: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """
    合并规则：runtime 覆盖 k8s（runtime 代表实时注册/权重覆盖）。
    """
    out: Dict[str, Dict[str, Any]] = {}

    for b in k8s:
        nb = normalize_backend(b, "k8s")
        out[key_of(nb)] = nb

    for b in runtime:
        nb = normalize_backend(b, "runtime")
        out[key_of(nb)] = nb

    res = [v for v in out.values() if v["ip"] and v["port"] > 0]
    res.sort(key=lambda x: (x["ip"], x["port"]))
    return res


def build_proxy_config(backends: List[Dict[str, Any]]) -> Dict[str, Any]:
    """生成给 C++ 代理读的 proxy_config.json（字段简洁稳定）。"""
    return {
        "listen": {"host": CFG["default_listen_host"], "port": CFG["default_listen_port"]},
        "algorithm": CFG["default_algorithm"],
        "updated_at": int(time.time()),
        "backends": [
            {
                "ip": b["ip"],
                "port": b["port"],
                "weight": b["weight"],
                "enabled": b["enabled"],
                "is_warming_up": b["is_warming_up"],
                # 下面两项是否给代理用，看你们 B 的实现需要；保留不影响
                "gpu_usage": b.get("gpu_usage", None),
                "vram_usage": b.get("vram_usage", None),
            }
            for b in backends
        ],
    }


# ------------------------- Step3：TTL + 自动同步 -------------------------

SYNC_LOCK = Lock()


def apply_runtime_ttl(rt_obj: Dict[str, Any]) -> Dict[str, Any]:
    """
    对 runtime backends 做 TTL：
    - 若 now - last_seen > ttl，则 enabled=false（只改 runtime 表，不动 k8s）。
    """
    ttl = int(CFG.get("runtime_ttl_sec", 0) or 0)
    if ttl <= 0:
        return rt_obj

    now = int(time.time())
    changed = False

    for b in rt_obj.get("backends", []):
        last_seen = int(b.get("last_seen", 0) or 0)
        if last_seen > 0 and (now - last_seen) > ttl:
            if bool(b.get("enabled", True)) is True:
                b["enabled"] = False
                changed = True

    if changed:
        rt_obj["updated_at"] = now
        write_json_atomic(CFG["runtime_backends_path"], rt_obj)

    return rt_obj


def sync_proxy_config_and_reload(reason: str) -> Dict[str, Any]:
    """
    统一同步入口：
    - 读取 k8s + runtime
    - runtime TTL 处理
    - merge -> 写 proxy_config.json
    - 尝试 SIGHUP 通知代理热重载
    """
    with SYNC_LOCK:
        k8s_obj = read_json(CFG["k8s_endpoints_path"], {"backends": [], "updated_at": 0})
        rt_obj = read_json(CFG["runtime_backends_path"], {"backends": [], "updated_at": 0})

        rt_obj = apply_runtime_ttl(rt_obj)

        merged = merge_backends(k8s_obj.get("backends", []), rt_obj.get("backends", []))
        proxy_cfg = build_proxy_config(merged)
        write_json_atomic(CFG["proxy_config_path"], proxy_cfg)

        ok, info = try_reload_proxy()

        return {
            "ok": True,
            "reason": reason,
            "proxy_config_path": abspath(CFG["proxy_config_path"]),
            "reload": {"ok": ok, "info": info},
            "merged_count": len(proxy_cfg.get("backends", [])),
            "k8s_updated_at": int(k8s_obj.get("updated_at", 0) or 0),
            "runtime_updated_at": int(rt_obj.get("updated_at", 0) or 0),
        }


def _watch_and_autosync():
    """
    轮询监控 k8s_endpoints/runtime_backends 文件变化，有变化就 sync + SIGHUP。
    这是 Step3 的核心：服务发现变化/注册变化 -> 自动同步到代理。
    """
    k8s_path = abspath(CFG["k8s_endpoints_path"])
    rt_path = abspath(CFG["runtime_backends_path"])

    last_mtime_k8s = 0
    last_mtime_rt = 0

    while True:
        try:
            m1 = int(os.path.getmtime(k8s_path)) if os.path.exists(k8s_path) else 0
            m2 = int(os.path.getmtime(rt_path)) if os.path.exists(rt_path) else 0

            if m1 != last_mtime_k8s or m2 != last_mtime_rt:
                last_mtime_k8s, last_mtime_rt = m1, m2
                res = sync_proxy_config_and_reload(reason="autosync:file_changed")
                # 控制台打印，演示/排查非常好用
                print(f"[autosync] merged={res['merged_count']} reload={res['reload']}")

        except Exception as e:
            print(f"[autosync] error: {e}")

        time.sleep(int(CFG.get("auto_sync_interval_sec", 2) or 2))


def start_autosync_thread():
    """启动后台 autosync 线程（daemon）。"""
    t = threading.Thread(target=_watch_and_autosync, daemon=True)
    t.start()


# ------------------------- Web UI 页面 -------------------------

@app.route("/")
def index():
    return render_template("index.html")


# ------------------------- API：查询后端 -------------------------

@app.route("/api/backends", methods=["GET"])
def api_backends():
    k8s_obj = read_json(CFG["k8s_endpoints_path"], {"backends": [], "updated_at": 0})
    rt_obj = read_json(CFG["runtime_backends_path"], {"backends": [], "updated_at": 0})

    # TTL 处理（保证你一刷新就看到超时节点已下线）
    rt_obj = apply_runtime_ttl(rt_obj)

    backends = merge_backends(k8s_obj.get("backends", []), rt_obj.get("backends", []))
    return jsonify(
        {
            "ok": True,
            "updated_at": int(time.time()),
            "k8s_updated_at": int(k8s_obj.get("updated_at", 0) or 0),
            "runtime_updated_at": int(rt_obj.get("updated_at", 0) or 0),
            "backends": backends,
        }
    )


# ------------------------- API：动态注册 -------------------------

@app.route("/api/register", methods=["POST"])
def api_register():
    """
    动态注册：
    请求 JSON:
      {"ip":"10.0.0.13","port":9000,"weight":10,"gpu_usage":0.2,"vram_usage":0.1}
    行为：
      upsert 到 data/backends_runtime.json
      - 新注册默认 is_warming_up=true
      - last_seen=now
      写完后立即 sync -> 写 proxy_config.json 并 SIGHUP
    """
    data = request.get_json(force=True, silent=True) or {}
    ip = str(data.get("ip", "")).strip()
    port = int(data.get("port", 0) or 0)

    weight = int(data.get("weight", 10) or 10)
    gpu = data.get("gpu_usage", None)
    vram = data.get("vram_usage", None)

    if not ip or port <= 0:
        return jsonify({"ok": False, "error": "invalid ip/port"}), 400

    rt_obj = read_json(
        CFG["runtime_backends_path"],
        {"source": "runtime_register", "updated_at": 0, "backends": []},
    )
    rt_list = rt_obj.get("backends", [])

    now = int(time.time())
    found = False
    for b in rt_list:
        if str(b.get("ip", "")) == ip and int(b.get("port", 0) or 0) == port:
            b["weight"] = weight
            b["gpu_usage"] = gpu
            b["vram_usage"] = vram
            b["enabled"] = True
            b["is_warming_up"] = bool(b.get("is_warming_up", True))
            b["last_seen"] = now
            found = True
            break

    if not found:
        rt_list.append(
            {
                "ip": ip,
                "port": port,
                "weight": weight,
                "gpu_usage": gpu,
                "vram_usage": vram,
                "enabled": True,
                "is_warming_up": True,  # 新注册默认先预热
                "last_seen": now,
            }
        )

    rt_obj["updated_at"] = now
    rt_obj["backends"] = rt_list
    write_json_atomic(CFG["runtime_backends_path"], rt_obj)

    sync_res = sync_proxy_config_and_reload(reason="register")
    return jsonify(
        {
            "ok": True,
            "message": "registered",
            "backend": {"ip": ip, "port": port},
            "sync": sync_res,
        }
    )


# ------------------------- API：修改权重（UI 保存按钮用） -------------------------

@app.route("/api/backends/<ip>/<int:port>/weight", methods=["POST"])
def api_set_weight(ip: str, port: int):
    """
    修改某后端 weight：
    - 写入 runtime 表（相当于覆盖配置）
    - 然后 sync -> 写 proxy_config.json 并 SIGHUP
    """
    data = request.get_json(force=True, silent=True) or {}
    weight = int(data.get("weight", 0) or 0)

    rt_obj = read_json(CFG["runtime_backends_path"], {"source": "runtime_register", "updated_at": 0, "backends": []})
    rt_list = rt_obj.get("backends", [])
    now = int(time.time())

    updated = False
    for b in rt_list:
        if str(b.get("ip", "")) == ip and int(b.get("port", 0) or 0) == port:
            b["weight"] = weight
            b["enabled"] = True
            b["last_seen"] = now
            updated = True
            break

    if not updated:
        # runtime 不存在该后端，也允许新增一条作为“覆盖配置”
        rt_list.append(
            {
                "ip": ip,
                "port": port,
                "weight": weight,
                "gpu_usage": None,
                "vram_usage": None,
                "enabled": True,
                "is_warming_up": False,
                "last_seen": now,
            }
        )

    rt_obj["updated_at"] = now
    rt_obj["backends"] = rt_list
    write_json_atomic(CFG["runtime_backends_path"], rt_obj)

    sync_res = sync_proxy_config_and_reload(reason="set_weight")
    return jsonify(
        {
            "ok": True,
            "updated": {"ip": ip, "port": port, "weight": weight},
            "sync": sync_res,
        }
    )


# ------------------------- API：手动同步（演示/调试） -------------------------

@app.route("/api/sync", methods=["POST"])
def api_sync():
    res = sync_proxy_config_and_reload(reason="manual")
    return jsonify(res)


# ------------------------- API：Ping 工具 -------------------------

@app.route("/api/ping", methods=["POST"])
def api_ping():
    """
    Ping 工具（用于演示诊断能力）
    请求 JSON: {"ip":"10.0.0.11"}
    """
    data = request.get_json(force=True, silent=True) or {}
    ip = str(data.get("ip", "")).strip()
    if not ip:
        return jsonify({"ok": False, "error": "missing ip"}), 400

    cmd = ["ping", "-c", "2", "-W", "1", ip]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
        out = (p.stdout or "") + (p.stderr or "")
        return jsonify({"ok": True, "output": out})
    except subprocess.TimeoutExpired:
        return jsonify({"ok": False, "output": "ping timeout"})
    except FileNotFoundError:
        return jsonify({"ok": False, "output": "ping command not found"})
    except Exception as e:
        return jsonify({"ok": False, "output": f"ping error: {e}"})


# ------------------------- API：审计日志 tail -------------------------

@app.route("/api/logs/audit", methods=["GET"])
def api_logs_audit():
    """
    审计日志查看（tail N 行）
    GET /api/logs/audit?tail=120
    """
    tail = int(request.args.get("tail", "100") or "100")
    tail = max(10, min(tail, 2000))
    data = tail_file(CFG["audit_log_path"], n=tail)
    return jsonify({"ok": True, "data": data, "path": abspath(CFG["audit_log_path"])})


# ------------------------- 启动入口 -------------------------

if __name__ == "__main__":
    # 确保目录存在
    os.makedirs(abspath("logs"), exist_ok=True)
    os.makedirs(abspath("data"), exist_ok=True)
    os.makedirs(abspath("config"), exist_ok=True)

    # debug 模式下 Flask 会启动两次（reloader），用环境变量保证线程只启动一次
    if os.environ.get("WERKZEUG_RUN_MAIN") == "true" or not app.debug:
        start_autosync_thread()

    app.run(host=CFG["host"], port=int(CFG["port"]), debug=True)
