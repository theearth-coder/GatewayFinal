#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Step4 - AI 批处理优化 (Mock)
模拟：如果 5 个“小请求”在同一批次到达，那么显存占用只 +1 单位（而不是 +5）
我们用 /api/register 上报 gpu_usage/vram_usage，让控制台可视化看到“批处理更省显存”。

用法：
  python scripts/sim_backend_register_batch.py --mode nobatch
  python scripts/sim_backend_register_batch.py --mode batch
"""

import argparse
import json
import random
import time
import urllib.request

def post_json(url, payload):
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type":"application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=3) as resp:
        return resp.read().decode("utf-8", errors="ignore")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="http://127.0.0.1:8080")
    ap.add_argument("--ip", default="10.0.0.13")
    ap.add_argument("--port", type=int, default=9000)
    ap.add_argument("--mode", choices=["nobatch", "batch"], default="batch")
    ap.add_argument("--interval", type=float, default=2.0, help="每轮上报间隔(秒)")
    ap.add_argument("--small_reqs", type=int, default=5, help="每轮模拟小请求数")
    args = ap.parse_args()

    # 把 vram_usage 看成 [0,1] 归一化指标
    vram = 0.10
    gpu = 0.20

    while True:
        # 模拟这一轮来了 N 个小请求
        n = args.small_reqs
        # 每个小请求理论上会让显存 +delta（无批处理）
        delta_each = 0.02 + random.random() * 0.01  # 0.02~0.03
        if args.mode == "nobatch":
            vram += n * delta_each
        else:
            # 批处理：5 个一起只算 1 份增量（“只增加 1 个单位”的降维演示）
            # 这里按“批大小=5”实现：n=5 时只 +1*delta_each
            batches = (n + 4) // 5
            vram += batches * delta_each

        # 再加入一点自然波动 + 衰减（模拟请求处理完释放显存）
        vram = max(0.0, min(1.0, vram - 0.03 + random.random() * 0.01))
        gpu = max(0.0, min(1.0, 0.3 + random.random() * 0.2))

        payload = {
            "ip": args.ip,
            "port": args.port,
            "weight": 10,
            "gpu_usage": round(gpu, 3),
            "vram_usage": round(vram, 3)
        }
        out = post_json(args.base + "/api/register", payload)
        print(f"[{args.mode}] small_reqs={n} vram={payload['vram_usage']} gpu={payload['gpu_usage']} -> {out[:80]}...")
        time.sleep(args.interval)

if __name__ == "__main__":
    main()
