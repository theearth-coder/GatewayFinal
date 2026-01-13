#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模拟 K8s/Consul 服务发现输出：生成 control/data/k8s_endpoints.json

用法：
  python scripts/gen_k8s_endpoints.py
  python scripts/gen_k8s_endpoints.py --loop --interval 5
"""
import argparse
import json
import os
import random
import time

DEFAULT_OUT = os.path.join(os.path.dirname(__file__), "..", "data", "k8s_endpoints.json")

def gen_backends(n=2):
    base_ip = "10.0.0."
    backends = []
    for i in range(n):
        ip = base_ip + str(11 + i)
        port = 9000
        gpu = max(0.0, min(1.0, random.gauss(0.5, 0.18)))
        vram = max(0.0, min(1.0, random.gauss(0.55, 0.2)))
        backends.append({
            "ip": ip,
            "port": port,
            "weight": 10,
            "gpu_usage": round(gpu, 3),
            "vram_usage": round(vram, 3),
            "enabled": True,
            "is_warming_up": False
        })
    return backends

def write_json_atomic(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)
    os.replace(tmp, path)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--n", type=int, default=2, help="后端数量")
    ap.add_argument("--loop", action="store_true", help="循环生成")
    ap.add_argument("--interval", type=int, default=5, help="循环间隔秒")
    args = ap.parse_args()

    while True:
        obj = {
            "source": "mock_k8s",
            "updated_at": int(time.time()),
            "backends": gen_backends(args.n)
        }
        write_json_atomic(args.out, obj)
        print(f"[gen_k8s_endpoints] wrote {args.out} backends={args.n}")

        if not args.loop:
            break
        time.sleep(args.interval)

if __name__ == "__main__":
    main()
