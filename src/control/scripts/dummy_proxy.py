#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
假代理：用于演示 SIGHUP 热重载链路
- 自动写入 ../run/proxy.pid
- 收到 SIGHUP 时打印提示
"""
import os, signal, time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PID_PATH = os.path.join(ROOT, "run", "proxy.pid")

def on_hup(sig, frame):
    print(">>> DUMMY PROXY GOT SIGHUP (reload)")

def main():
    os.makedirs(os.path.dirname(PID_PATH), exist_ok=True)
    pid = os.getpid()
    with open(PID_PATH, "w", encoding="utf-8") as f:
        f.write(str(pid))
    print(f"[dummy_proxy] pid={pid} wrote {PID_PATH}")

    signal.signal(signal.SIGHUP, on_hup)
    while True:
        time.sleep(1)

if __name__ == "__main__":
    main()
