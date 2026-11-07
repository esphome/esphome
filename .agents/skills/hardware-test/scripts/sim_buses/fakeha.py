"""Stand in for Home Assistant against homeassistant select mirrors.

usage: fakeha.py HOST OUT_PREFIX
Plaintext API on port 6053 (no encryption key in the DUT config). Answers the device's state subscriptions from a local table, pushes option and state changes, presses the
"Dump" button after each step and records device logs, select states and action calls. The DUT has two
homeassistant selects, "Small" (select.speed, 4 options / 16 bytes of storage) and "Big" (input_select.mode,
16 options / 240 bytes), and a "Dump" button logging "DUMP <name> n=<count> state=<state> opts=<a|b|c>" for each
plus a "HEAP" line whose first number is the free heap in bytes. A step fails when either DUMP line is missing, and
a rejected update also needs the device's rejection log line. Exits 1 when a step fails, including a churn that loses
more than HEAP_TOLERANCE bytes of free heap.
"""

import asyncio
import pathlib
import re
import sys
import time

from aioesphomeapi import APIClient, ButtonInfo, LogLevel, SelectInfo, SelectState

DEV, OUT = sys.argv[1], sys.argv[2]
SMALL, BIG = "select.speed", "input_select.mode"
HEAP_TOLERANCE = 1024
table = {
    (SMALL, "options"): repr(["low", "medium", "high"]),
    (SMALL, None): "medium",
    (BIG, "options"): repr(["Home", "Away", "Night"]),
    (BIG, None): "Away",
}
logf = pathlib.Path(f"{OUT}-log.txt").open("w", encoding="utf-8")  # noqa: SIM115
events = pathlib.Path(f"{OUT}-events.txt").open("w", encoding="utf-8")  # noqa: SIM115
lines: list[str] = []
calls: list[tuple[str, dict]] = []
states: dict[int, str | None] = {}
subs: list[tuple[str, str | None]] = []
fails: list[str] = []
DUMP = re.compile(r"DUMP (\w+) n=\d+ state=(.*?) opts=(.*)$")


def ev(msg: str) -> None:
    print(msg, flush=True)
    events.write(msg + "\n")
    events.flush()


def on_log(msg) -> None:
    text = re.sub(r"\x1b\[[0-9;]*m", "", msg.message.decode(errors="replace"))
    lines.append(text)
    logf.write(f"{time.monotonic():.3f} {text}\n")
    logf.flush()


async def main() -> None:
    cli = APIClient(DEV, 6053, None)
    await cli.connect(login=True)
    ents, _ = await cli.list_entities_services()
    dump = next(e.key for e in ents if isinstance(e, ButtonInfo) and e.name == "Dump")
    sel = {e.name: e for e in ents if isinstance(e, SelectInfo)}
    ev(f"ENTITY_OPTIONS at connect {[(n, e.options) for n, e in sel.items()]}")
    names = {e.key: n for n, e in sel.items()}
    cli.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)

    def on_state(s) -> None:
        if isinstance(s, SelectState):
            states[s.key] = None if s.missing_state else s.state

    def on_service(c) -> None:
        calls.append((c.service, dict(c.data)))

    def on_sub(entity_id: str, attribute: str | None) -> None:
        attribute = attribute or None
        subs.append((entity_id, attribute))
        if (val := table.get((entity_id, attribute))) is not None:
            cli.send_home_assistant_state(entity_id, attribute, val)

    cli.subscribe_home_assistant_states_and_services(on_state, on_service, on_sub)
    await asyncio.sleep(2)
    ev(f"SUBS {subs}")

    async def step(
        name: str, sends=(), expect_small=None, expect_big=None, evidence=None
    ) -> None:
        # evidence: regex a device log line from this step must match, so a rejected update proves it arrived
        mark = len(lines)
        for ent, attr, val in sends:
            table[(ent, attr)] = val
            cli.send_home_assistant_state(ent, attr, val)
        await asyncio.sleep(1.0)
        for _ in range(3):
            dmark = len(lines)
            cli.button_command(dump)
            deadline = time.monotonic() + 1.5
            while True:
                dumps = {
                    m.group(1): (m.group(2), m.group(3))
                    for line in lines[dmark:]
                    if (m := re.search(DUMP, line))
                }
                done = len(dumps) == 2 and any("HEAP" in line for line in lines[dmark:])
                if done or time.monotonic() > deadline:
                    break
                await asyncio.sleep(0.05)
            if len(dumps) == 2:
                break
        new = [
            line
            for line in lines[mark:]
            if "homeassistant.select" in line or "DUMP" in line or "HEAP" in line
        ]
        ok = (
            len(dumps) == 2
            and all(
                exp is None or dumps.get(nm) == exp
                for nm, exp in (("Small", expect_small), ("Big", expect_big))
            )
            and (evidence is None or any(re.search(evidence, line) for line in new))
        )
        api = {names[k]: v for k, v in states.items()}
        if not ok:
            fails.append(name)
        ev(f"STEP {name}: {'PASS' if ok else 'FAIL'} dumps={dumps} api_states={api}")
        for line in new:
            if "DUMP" not in line:
                ev(
                    "    " + line[line.find("]:") + 2 :].strip()
                    if "]:" in line
                    else line
                )

    await step(
        "initial",
        expect_small=("medium", "low|medium|high"),
        expect_big=("Away", "Home|Away|Night"),
    )
    await step(
        "small reordered, active moves",
        [(SMALL, "options", repr(["high", "medium", "off"]))],
        expect_small=("medium", "high|medium|off"),
    )
    await step(
        "small exactly full 4 options 16 bytes",
        [(SMALL, "options", repr(["aaa", "bbb", "ccc", "ddd"])), (SMALL, None, "ccc")],
        expect_small=("ccc", "aaa|bbb|ccc|ddd"),
    )
    await step(
        "small 5 options rejected",
        [(SMALL, "options", repr(["a", "b", "c", "d", "e"]))],
        expect_small=("ccc", "aaa|bbb|ccc|ddd"),
        evidence=rf"'{SMALL}': 5 options exceed max_options",
    )
    await step(
        "small 17 bytes rejected",
        [(SMALL, "options", repr(["aaaaa", "bbbbb", "cccc"]))],
        expect_small=("ccc", "aaa|bbb|ccc|ddd"),
        evidence=rf"'{SMALL}': Options need 17 bytes",
    )
    await step(
        "small unparseable rejected",
        [(SMALL, "options", "not a list")],
        expect_small=("ccc", "aaa|bbb|ccc|ddd"),
        evidence=rf"'{SMALL}': Can't parse options",
    )
    await step(
        "small active removed",
        [(SMALL, "options", repr(["aaa", "bbb", "ddd"]))],
        expect_small=("<none>", "aaa|bbb|ddd"),
    )
    await step(
        "small state not an option",
        [(SMALL, None, "zzz")],
        expect_small=("<none>", "aaa|bbb|ddd"),
        evidence=rf"'{SMALL}': State 'zzz' is not one of the options",
    )
    await step(
        "small state set", [(SMALL, None, "bbb")], expect_small=("bbb", "aaa|bbb|ddd")
    )
    quoted = ["it's", "a,b", 'x"y', "Night"]
    await step(
        "big quoted options",
        [(BIG, "options", repr(quoted))],
        expect_big=("<none>", "|".join(quoted)),
        expect_small=("bbb", "aaa|bbb|ddd"),
    )
    full = [
        f"option-{i:02d}-" + "x" * 4 for i in range(16)
    ]  # 16 options, 14 characters plus NUL each = 240 bytes
    await step(
        "big 16 long options",
        [(BIG, "options", repr(full)), (BIG, None, full[15])],
        expect_big=(full[15], "|".join(full)),
        expect_small=("bbb", "aaa|bbb|ddd"),
    )
    await step(
        "big 17 options rejected",
        [(BIG, "options", repr(full + ["extra"]))],
        expect_big=(full[15], "|".join(full)),
        evidence=rf"'{BIG}': 17 options exceed max_options",
    )

    calls.clear()
    cli.select_command(sel["Small"].key, "ddd")
    await asyncio.sleep(2)
    cli.select_command(sel["Big"].key, full[3])
    await asyncio.sleep(3)
    want = [
        ("select.select_option", {"entity_id": SMALL, "option": "ddd"}),
        ("input_select.select_option", {"entity_id": BIG, "option": full[3]}),
    ]
    if calls != want:
        fails.append("select_command")
    ev(f"STEP select_command: {'PASS' if calls == want else 'FAIL'} calls={calls}")

    # Churn: many option rewrites; free heap must not move
    mark = len(lines)
    cli.button_command(dump)
    await asyncio.sleep(1)
    for i in range(300):
        a = [f"s{i % 7}", "medium", f"t{i % 3}"]
        b = [f"opt{i}-{j}" for j in range(1 + i % 16)]
        cli.send_home_assistant_state(SMALL, "options", repr(a))
        cli.send_home_assistant_state(BIG, "options", repr(b))
        if i % 20 == 0:
            await asyncio.sleep(0.2)
    await asyncio.sleep(2)
    cli.send_home_assistant_state(SMALL, "options", repr(["aaa", "bbb", "ddd"]))
    cli.send_home_assistant_state(BIG, "options", repr(full))
    await asyncio.sleep(1)
    cli.button_command(dump)
    await asyncio.sleep(1)
    heaps = [line[line.find("HEAP") :] for line in lines[mark:] if "HEAP" in line]
    got = sum(1 for line in lines[mark:] if "Got " in line and "options" in line)
    free = [int(m.group(1)) for h in heaps if (m := re.search(r"(\d+)", h))]
    ok = got > 0 and len(free) >= 2 and free[0] - free[-1] <= HEAP_TOLERANCE
    if not ok:
        fails.append("churn")
    ev(
        f"STEP churn 600 option updates: {'PASS' if ok else 'FAIL'} got_options_lines={got} heap={heaps}"
    )
    await cli.disconnect()

    # Reconnect: entity info carries the options the traits point at
    cli2 = APIClient(DEV, 6053, None)
    await cli2.connect(login=True)
    ents, _ = await cli2.list_entities_services()
    opts = {e.name: e.options for e in ents if isinstance(e, SelectInfo)}
    ok = opts.get("Small") == ["aaa", "bbb", "ddd"] and opts.get("Big") == full
    if not ok:
        fails.append("reconnect")
    ev(f"STEP reconnect entity options: {'PASS' if ok else 'FAIL'} {opts}")
    await cli2.disconnect()


asyncio.run(main())
if fails:
    print(f"FAIL: {fails}")
    sys.exit(1)
print("PASS")
