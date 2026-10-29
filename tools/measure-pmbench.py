#!/usr/bin/env python3
"""Host wall-clock batches over telnet; includes transport/TTY overhead.

Requires Python 3.9-3.12 (telnetlib). Run pmbench only in a fresh USB directory.
No clock correction or subtraction: noop samples show transport overhead.
"""
import argparse
import json
import re
import secrets
import statistics
import time
import telnetlib
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("host")
    p.add_argument("directory", help="fresh /mnt/usb/NAME, 1-8 uppercase letters/digits")
    p.add_argument("output", type=Path)
    p.add_argument("--program", default="/root/pmbench")
    args = p.parse_args()
    if not re.fullmatch(r"/mnt/usb/[A-Z0-9]{1,8}", args.directory):
        p.error("invalid fresh test directory")
    if not re.fullmatch(r"/[A-Za-z0-9/_-]+", args.program):
        p.error("invalid program path")
    if args.output.exists():
        p.error("output already exists")
    samples = []
    result = dict(status="incomplete", clock="host time.perf_counter_ns",
                  metric="batch wall time including transport and TTY; not firmware service time",
                  directory=args.directory, samples=samples)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    try:
        with telnetlib.Telnet(args.host, 23, 5) as t:
            if not t.read_until(b"login: ", 10).endswith(b"login: "):
                raise RuntimeError("No login prompt")
            t.write(b"root\r\n")
            marker = ("PMREADY_" + secrets.token_hex(6)).encode()
            t.write(b"echo " + marker + b"\r\n")
            if not t.read_until(b"\n" + marker + b"\r\n", 15).endswith(marker + b"\r\n"):
                raise RuntimeError("No root shell")
            t.write((args.program + " " + args.directory + "\r\n").encode())
            while True:
                index, match, raw = t.expect([
                    rb"PMBENCH READY ([0-9]+) ([a-z0-9_]+) ([0-9]+)\r?\n",
                    rb"PMBENCH PASS [^\r\n]+\r?\n",
                    rb"PMBENCH FAIL [^\r\n]+\r?\n"], timeout=60)
                if index == 1:
                    if len(samples) != 18:
                        raise RuntimeError("Incomplete sample count")
                    result["status"] = "pass"
                    result["completion"] = match[0].decode().strip()
                    t.write(b"exit\r\n")
                    break
                if index != 0:
                    raise RuntimeError("Workload failed or timed out: " + raw.decode("latin1")[-500:])
                seq, op, count = int(match[1]), match[2].decode(), int(match[3])
                if seq != len(samples) + 1 or count != 4:
                    raise RuntimeError("Unexpected workload sequence")
                # Let preceding output/ACKs drain. This delay is outside the measurement.
                time.sleep(0.2)
                before = time.perf_counter_ns()
                t.write(b"go\r\n")
                end = ("PMBENCH DONE %d\r\n" % seq).encode()
                raw = t.read_until(end, 60)
                elapsed = time.perf_counter_ns() - before
                if not raw.endswith(end):
                    raise RuntimeError("Batch failed or timed out: " + raw.decode("latin1")[-500:])
                sample = dict(sequence=seq, operation=op, iterations=count,
                              elapsed_ns=elapsed, ms_per_iteration=elapsed / count / 1e6)
                samples.append(sample)
                print("%s batch %d: %.3f ms/iteration (includes transport)" %
                      (op, seq, sample["ms_per_iteration"]), flush=True)
                args.output.write_text(json.dumps(result, indent=2) + "\n")
    except Exception as exc:
        result["error"] = str(exc)
        raise
    finally:
        result["median_ms_per_iteration"] = {
            op: statistics.median(s["ms_per_iteration"] for s in samples if s["operation"] == op)
            for op in dict.fromkeys(s["operation"] for s in samples)}
        args.output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
