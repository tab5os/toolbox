"""Small helper for driving the Tab5 over its USB console from tests.

    python3 tests/t5do.py install            install build/<id>.t5app
    python3 tests/t5do.py shot OUT.png        screenshot
    python3 tests/t5do.py cmd "app" "sys"     console commands
    python3 tests/t5do.py log SECONDS         print the console output for a while

Uses the SDK's own USB link (tools/t5/t5link.py) from .t5sdk/, and needs
pyserial (.t5sdk/venv, made by tests/device_test.sh).
"""
import base64
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDK_TOOLS = sorted(glob.glob(os.path.join(HERE, ".t5sdk", "tab5os-sdk-*", "tools", "t5")))[-1]
sys.path.insert(0, SDK_TOOLS)

import t5img   # noqa: E402
import t5link  # noqa: E402

APP_ID = "com.t5.example.toolbox"
PKG = os.path.join(HERE, "build", APP_ID + ".t5app")


def install(t):
    with open(PKG, "rb") as f:
        data = f.read()
    return t.install(data, APP_ID)


B64 = re.compile(r"^[A-Za-z0-9+/=]+")


def parse_dump(text):
    """Like t5img.parse_console_dump, but rejoins a base64 line that another
    task's log output (the OS's task watchdog fires during a long `shot 2`)
    split in two: the head ends where the log starts, the tail is the next
    short base64 line after the log."""
    rot, w, h, data, inside, head = 0, 0, 0, bytearray(), False, ""
    full = 64
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("rotation "):
            rot = int(line.split()[1])
        elif line.startswith("SHOT "):
            _, w, h = line.split()
            w, h, inside = int(w), int(h), True
        elif line == "END":
            inside = False
        elif inside and line:
            m = B64.match(line)
            last = line.split()[-1]
            if (not m or m.end() < len(line)) and len(last) == full and B64.fullmatch(last):
                # A log line written in two parts with a picture line between
                # them: "[Warn] ... indev_pointer_proc: <base64>" then "Y is ...".
                data += base64.b64decode(last)
            elif m and m.end() == len(line) and len(line) == full:
                data += base64.b64decode(line)
            elif m and m.end() == len(line) and head and len(head) + len(line) == full:
                data += base64.b64decode(head + line)
                head = ""
            elif m and m.end() < len(line) and not head:
                head = m.group(0)
            elif m and m.end() == len(line) and len(line) % 4 == 0 and len(line) < full:
                data += base64.b64decode(line)          # the last line
    if not w or len(data) < w * h * 2:
        raise ValueError("incomplete dump: %d of %d bytes" % (len(data), w * h * 2))
    return rot, w, h, bytes(data[:w * h * 2])


def shot(t, out, div=4):
    """A screenshot at 1/div size. At 1/2 the dump takes long enough to trip
    the OS's task watchdog, so 1/4 is the default; a damaged 1/2 dump falls
    back to 1/4."""
    tries = [div] + [4] * 3
    for i, d in enumerate(tries):
        try:
            reply = t.cmd(f"shot {d}", 90)
            # App lines printed during the dump: keep them for whoever reads next.
            kept = [ln for ln in reply.splitlines() if "toolbox: " in ln]
            if kept:
                t._rx = ("\n".join(ln[ln.index("toolbox: "):] for ln in kept) + "\n").encode() + t._rx
            rot, w, h, data = parse_dump(reply)
            break
        except ValueError:          # lines lost on the way: take it again
            if i == len(tries) - 1:
                raise
    ow, oh = t5img.save(out, data, w, h, rot)
    return ow, oh, rot


def wshot(out, host=None):
    """A screenshot over Wi-Fi from a Tab5 in developer mode, with the token
    `t5 pair` saved (~/.config/t5/devices.json). host: the Tab5's address
    now, if it changed since pairing ($T5_HOST also works)."""
    import json
    import urllib.request
    with open(os.path.expanduser("~/.config/t5/devices.json")) as f:
        cfg = json.load(f)
    paired = cfg.get("default")
    host = host or os.environ.get("T5_HOST") or paired
    token = (cfg["devices"].get(host) or cfg["devices"].get(paired) or {}).get("token")
    req = urllib.request.Request(f"http://{host}:8350/api/screenshot?div=1",
                                 headers={"Authorization": "Bearer " + token} if token else {})
    with urllib.request.urlopen(req, timeout=30) as r:
        data, h = r.read(), r.headers
    return t5img.save(out, data, int(h["X-Width"]), int(h["X-Height"]), int(h["X-Rotation"]))


def main():
    op, args = sys.argv[1], sys.argv[2:]
    if op == "wshot":
        print(wshot(args[0], args[1] if len(args) > 1 else None))
        return
    with t5link.Tab5(timeout=60) as t:
        if op == "install":
            print(install(t))
        elif op == "shot":
            print(shot(t, args[0], int(args[1]) if len(args) > 1 else 4))
        elif op == "cmd":
            for c in args:
                print(f"### {c}\n{t.cmd(c, 120)}")
        elif op == "log":
            sys.stdout.write(t.read_for(float(args[0])).decode(errors="replace"))


if __name__ == "__main__":
    main()
