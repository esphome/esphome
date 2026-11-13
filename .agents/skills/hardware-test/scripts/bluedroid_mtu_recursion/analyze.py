"""Count rounds, BTU_TASK crashes, reconnects without reboot and MTU 517 in a central capture.

analyze.py CAPTURE
"""

import pathlib
import re
import sys

lines = (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
)
rounds = crashes = other_panics = boots = connects = mtu517 = mtu_other = 0
reconnect_ok = reconnect_reboot = 0
pending = False  # a round fired and we wait for the next connect
for ln in lines:
    if "Stack canary watchpoint triggered (BTU_TASK)" in ln:
        crashes += 1
    elif "Guru Meditation" in ln or "abort() was called" in ln:
        other_panics += 1
    if "rst:0x" in ln:
        boots += 1
        if pending:
            reconnect_reboot += 1
            pending = False
    if "CLI connected" in ln:
        connects += 1
        if pending:
            reconnect_ok += 1
            pending = False
    if re.search(r"ROUND \d+ start", ln):
        rounds += 1
        pending = True
    if m := re.search(r"cfg_mtu status 0, mtu (\d+)", ln):
        if m.group(1) == "517":
            mtu517 += 1
        else:
            mtu_other += 1
print(
    f"rounds={rounds} btu_canary_crashes={crashes} other_panics={other_panics} boots={boots}"
)
print(
    f"connects={connects} reconnect_without_reboot={reconnect_ok} reconnect_after_reboot={reconnect_reboot}"
)
print(f"mtu517={mtu517} mtu_other={mtu_other}")
