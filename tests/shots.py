"""Open tools on a Tab5 by launch argument and screenshot each one, with no
touch input. Needs the Tab5 on USB-C (console) and in developer mode on
Wi-Fi (screenshots).

    .t5sdk/venv/bin/python3 tests/shots.py HOST [--install] [--rot DEG] [tool.id ...]

Screenshots go to build/shots/<tool.id>[-DEG].png. The screen is kept on for
the run and released afterwards; with --rot the display is turned for the
run and put back as it was.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import t5do  # noqa: E402

APP = t5do.APP_ID
ALL = ["home", "conv.temp", "conv.fuel", "num.base", "num.pct", "num.ratio", "num.random", "num.password",
       "money.tip", "money.discount", "money.unit", "elec.ohm", "elec.resistor", "elec.led", "elec.divider",
       "elec.battery", "date.diff", "date.add", "date.weekday", "date.age", "time.stopwatch", "time.countdown",
       "colour.convert", "measure.ruler", "measure.level", "measure.protractor", "measure.sound"]


def main():
    args = sys.argv[1:]
    host = args.pop(0)
    install = "--install" in args
    rot = None
    if "--rot" in args:
        i = args.index("--rot")
        rot = int(args[i + 1])
        del args[i:i + 2]
    tools = [a for a in args if a != "--install"] or ALL
    out = os.path.join(t5do.HERE, "build", "shots")
    os.makedirs(out, exist_ok=True)
    start = time.time()
    with t5do.t5link.Tab5(timeout=60) as t:
        if install:
            print(t5do.install(t).strip().splitlines()[-1])
        t.cmd("power wake")
        t.cmd("power lock screen")
        t.cmd("shell standby off")
        old_rot = None
        if rot is not None:
            m = re.search(r"rotation (\d+)", t.cmd("ui", 10))
            old_rot = m.group(1) if m else None
            t.cmd(f"ui rot {rot}", 10)
            time.sleep(1)
        try:
            for tool in tools:
                reply = t.cmd(f"app run {APP} {tool}", 20)
                if "crash" in reply:
                    print(tool, "CRASH", reply)
                time.sleep(1.2)
                path = os.path.join(out, tool + (f"-{rot}" if rot is not None else "") + ".png")
                try:
                    print(tool, t5do.wshot(path, host))
                except Exception as e:  # noqa: BLE001
                    print(tool, "screenshot failed:", e)
            print(t.cmd("app crashes", 10).strip()[-400:])
            print(t.cmd("app usage", 10).strip()[-900:])
        finally:
            if old_rot is not None:
                t.cmd(f"ui rot {old_rot}", 10)
                print("rotation back to", old_rot, t.cmd("ui", 10).strip().splitlines()[0])
            t.cmd("power unlock screen")
    print("took %.0f s" % (time.time() - start))


if __name__ == "__main__":
    main()
