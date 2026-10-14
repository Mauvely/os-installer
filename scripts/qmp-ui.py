#!/usr/bin/env python3
"""qmp-ui.py <socket> <command> [args]   Drives the VM's screen over QMP.

  shot <out.png>        screendump, PNG
  click <x> <y>         left click at screen pixels (1280x800, a usb-tablet)
  type <text>           types text, US layout; a trailing \\n is Enter
  key <qcode>...        presses keys together, e.g. `key tab`, `key shift tab`
"""
import json, socket, sys, time

W, H = 1280, 800

s = socket.socket(socket.AF_UNIX)
s.connect(sys.argv[1])
f = s.makefile("rw")
f.readline()


def call(cmd, args=None):
    f.write(json.dumps({"execute": cmd, **({"arguments": args} if args else {})}) + "\n")
    f.flush()
    while True:
        r = json.loads(f.readline())
        if "return" in r:
            return r["return"]
        if "error" in r:
            sys.exit(f"{cmd}: {r['error']}")


call("qmp_capabilities")

PLAIN = {" ": "spc", "-": "minus", "=": "equal", "/": "slash", ".": "dot", ",": "comma", ";": "semicolon",
         "\\": "backslash", "'": "apostrophe", "[": "bracket_left", "]": "bracket_right", "\n": "ret"}
SHIFT = {"_": "minus", "+": "equal", ">": "dot", "<": "comma", ":": "semicolon", "|": "backslash", "*": "8",
         "(": "9", ")": "0", "&": "7", "$": "4", "\"": "apostrophe", "{": "bracket_left", "}": "bracket_right",
         "!": "1", "@": "2", "#": "3", "%": "5", "^": "6", "?": "slash"}

cmd, args = sys.argv[2], sys.argv[3:]
if cmd == "shot":
    call("screendump", {"filename": args[0], "format": "png"})
elif cmd == "click":
    x, y = int(args[0]), int(args[1])
    pos = [{"type": "abs", "data": {"axis": "x", "value": x * 32767 // (W - 1)}},
           {"type": "abs", "data": {"axis": "y", "value": y * 32767 // (H - 1)}}]
    call("input-send-event", {"events": pos})
    time.sleep(0.1)
    call("input-send-event", {"events": [{"type": "btn", "data": {"down": True, "button": "left"}}]})
    time.sleep(0.05)
    call("input-send-event", {"events": [{"type": "btn", "data": {"down": False, "button": "left"}}]})
elif cmd == "type":
    for ch in args[0].replace("\\n", "\n"):
        if ch in PLAIN:
            keys = [PLAIN[ch]]
        elif ch in SHIFT:
            keys = ["shift", SHIFT[ch]]
        elif ch.isupper():
            keys = ["shift", ch.lower()]
        else:
            keys = [ch]
        call("send-key", {"keys": [{"type": "qcode", "data": k} for k in keys]})
        time.sleep(0.04)
elif cmd == "key":
    call("send-key", {"keys": [{"type": "qcode", "data": k} for k in args]})
else:
    sys.exit(f"unknown command {cmd}")
