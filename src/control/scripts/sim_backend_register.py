#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模拟后端周期上报 /api/register（用于 Step3 演示）
"""
import argparse, json, random, time, urllib.request

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
    ap.add_argument("--interval", type=int, default=3)
    args = ap.parse_args()

    while True:
        gpu = max(0.0, min(1.0, random.gauss(0.5, 0.2)))
        vram = max(0.0, min(1.0, random.gauss(0.6, 0.2)))
        payload = {
            "ip": args.ip,
            "port": args.port,
            "weight": 10,
            "gpu_usage": round(gpu, 3),
            "vram_usage": round(vram, 3)
        }
        out = post_json(args.base + "/api/register", payload)
        print("[register]", payload, "->", out[:120], "...")
        time.sleep(args.interval)

if __name__ == "__main__":
    main()
