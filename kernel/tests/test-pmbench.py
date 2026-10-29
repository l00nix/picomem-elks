#!/usr/bin/env python3
"""Exercise workload, protocol and refusal to reuse an existing directory."""
import os
import pathlib
import subprocess
import tempfile

source = pathlib.Path(__file__).resolve().parents[1] / "pmbench.c"
with tempfile.TemporaryDirectory(prefix="pmbench-", dir=os.environ.get("TMPDIR", "/tmp")) as tmp:
    root = pathlib.Path(tmp)
    binary = root / "pmbench"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra",
                    "-Werror", "-fsanitize=address,undefined", "-DPMBENCH_HOST_TEST",
                    str(source), "-o", str(binary)], check=True)
    fixture = root / "NEWTEST"
    p = subprocess.Popen([str(binary), fixture.name], stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=root)
    ops = ["noop", "read3073", "write3073", "rename_pair", "create_unlink", "mkdir_rmdir"]
    for seq in range(1, 19):
        assert p.stdout.readline().strip() == "PMBENCH READY %d %s 4" % (seq, ops[(seq-1)//3])
        p.stdin.write("go\n")
        p.stdin.flush()
        assert p.stdout.readline().strip() == "PMBENCH DONE %d" % seq
    out, err = p.communicate(timeout=10)
    assert p.returncode == 0 and "PMBENCH PASS 18 batches" in out, (out, err)
    data = bytes((j * 17 + 23) & 255 for j in range(3073))
    assert list(fixture.iterdir()) == [fixture / "Runtime File Alpha.txt"]
    assert (fixture / "Runtime File Alpha.txt").read_bytes() == data
    refused = subprocess.run([str(binary), fixture.name], capture_output=True, cwd=root)
    assert refused.returncode == 1 and b"NEW directory" in refused.stderr
    assert (fixture / "Runtime File Alpha.txt").read_bytes() == data
    for invalid in ("bad", "TOOLONG123", ".."):
        assert subprocess.run([str(binary), invalid], capture_output=True, cwd=root).returncode == 2
    aborted = subprocess.run([str(binary), "ABORT"], input=b"wrong\n", capture_output=True, cwd=root)
    assert aborted.returncode == 1 and b"host protocol" in aborted.stderr
    assert (root/"ABORT/Runtime File Alpha.txt").read_bytes() == data
print("PASS pmbench: 18 batches, exact payload, existing-directory/path/protocol guards")
