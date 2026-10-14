#!/usr/bin/env python3
"""Minimal QMP client: qmp.py <socket> <command> [json-arguments]
screendump to PNG: qmp.py <socket> screendump '{"filename":"/x/y.ppm"}' (convert after)."""
import json, socket, sys
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1])
f = s.makefile("rw"); f.readline()
def call(cmd, args=None):
    f.write(json.dumps({"execute": cmd, **({"arguments": args} if args else {})}) + "\n"); f.flush()
    while True:
        r = json.loads(f.readline())
        if "return" in r or "error" in r: return r
call("qmp_capabilities")
print(json.dumps(call(sys.argv[2], json.loads(sys.argv[3]) if len(sys.argv) > 3 else None)))
