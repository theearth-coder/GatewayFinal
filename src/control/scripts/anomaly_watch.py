#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
审计日志异常检测（简化版）：
- 针对“新增日志”统计 5xx 比例，超过阈值告警
- 同类告警 5 分钟冷却

假设 audit.log 每行格式类似（建议 C++ 代理输出）：
  2026-01-06T12:00:01Z src=1.2.3.4 method=GET url=/ status=200 latency_ms=12 backend=10.0.0.11:9000
"""
import argparse
import os
import re
import time
import json
from collections import deque

LINE_RE = re.compile(r"status=(\d{3})")

def tail_lines(path, max_lines=2000):
    if not os.path.exists(path):
        return []
    dq = deque(maxlen=max_lines)
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            dq.append(line.rstrip("\n"))
    return list(dq)

def write_append(path, msg):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "a", encoding="utf-8") as f:
        f.write(msg + "\n")

def load_state(path):
    if not os.path.exists(path):
        return {}
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}

def save_state(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)
    os.replace(tmp, path)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--audit", default=os.path.join(os.path.dirname(__file__), "..", "..", "logs", "audit.log"))
    ap.add_argument("--alert", default=os.path.join(os.path.dirname(__file__), "..", "logs", "alert.log"))
    ap.add_argument("--state", default=os.path.join(os.path.dirname(__file__), "..", "data", "alert_state.json"))
    ap.add_argument("--threshold", type=float, default=0.2, help="5xx 比例阈值（针对新增日志）")
    ap.add_argument("--cooldown", type=int, default=300, help="冷却秒（默认 5 分钟）")
    ap.add_argument("--interval", type=int, default=10, help="检测间隔")
    args = ap.parse_args()

    last_snapshot_len = 0
    state = load_state(args.state)

    while True:
        lines = tail_lines(args.audit, max_lines=4000)
        new_lines = lines[last_snapshot_len:] if last_snapshot_len <= len(lines) else lines
        last_snapshot_len = len(lines)

        total = 0
        err5xx = 0
        for ln in new_lines:
            m = LINE_RE.search(ln)
            if not m:
                continue
            total += 1
            code = int(m.group(1))
            if 500 <= code <= 599:
                err5xx += 1

        if total > 0:
            ratio = err5xx / total
            now = int(time.time())
            key = "5xx_spike"
            last_ts = int(state.get(key, 0))

            if ratio >= args.threshold and (now - last_ts) >= args.cooldown:
                msg = f"[ALERT] {time.strftime('%F %T')} 5xx_ratio={ratio:.3f} ({err5xx}/{total}) audit={args.audit}"
                print(msg)
                write_append(args.alert, msg)
                state[key] = now
                save_state(args.state, state)
            else:
                print(f"[OK] {time.strftime('%F %T')} total={total} 5xx={err5xx} ratio={ratio:.3f}")
        else:
            print(f"[OK] {time.strftime('%F %T')} no new status lines")

        time.sleep(args.interval)

if __name__ == "__main__":
    main()
