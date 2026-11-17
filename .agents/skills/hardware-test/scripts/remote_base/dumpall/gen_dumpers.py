"""gen_dumpers.py TREE [OUT]: write OUT (default dumpers.h here) for rbh.hpp, one dumper per DECLARE_REMOTE_PROTOCOL(...) in TREE's
remote_base plus Pronto, RCSwitch and ABBWelcome (Raw is called by rbh.hpp only when nothing matched)."""

import pathlib
import re
import sys

rb = pathlib.Path(sys.argv[1]) / "esphome" / "components" / "remote_base"
headers, names = [], []
for h in sorted(rb.glob("*_protocol.h")):
    found = re.findall(
        r"^DECLARE_REMOTE_PROTOCOL\((\w+)\)",
        h.read_text(encoding="utf-8"),
        re.MULTILINE,
    )
    if found or h.name in (
        "pronto_protocol.h",
        "rc_switch_protocol.h",
        "abbwelcome_protocol.h",
        "raw_protocol.h",
    ):
        headers.append(h.name)
    names += found
names += ["Pronto", "RCSwitch", "ABBWelcome"]
out = ["#pragma once"]
out += [f'#include "esphome/components/remote_base/{h}"' for h in headers]
out += [
    "using namespace esphome::remote_base;",
    "struct RbDumper {",
    "  const char *name;",
    "  RemoteReceiverDumperBase *d;",
    "};",
]
out += [f"static {n}Dumper rbd_{n.lower()};" for n in names]
out += ["static RbDumper RB_DUMPERS[] = {"]
out += [f'    {{"{n}", &rbd_{n.lower()}}},' for n in names]
out += ["};", ""]
out_path = (
    pathlib.Path(sys.argv[2])
    if len(sys.argv) > 2
    else pathlib.Path(__file__).with_name("dumpers.h")
)
out_path.write_text("\n".join(out), encoding="utf-8")
print(len(names), "dumpers")
