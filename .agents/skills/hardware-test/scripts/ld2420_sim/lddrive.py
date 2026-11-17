"""Record the LD2420 entity states over the native API while the fake radar's inputs play.

usage: lddrive.py HOST BUTTON OUT   (plaintext API; BUTTON is "Energy steps" or "Simple steps")

Exits 3 when the device cannot be reached, so the runner retries only that; any other error is a harness error.

Presses "Sim stats" (logs the commands answered during setup), then BUTTON, and writes every state and log line with
its time to OUT. compare.py diffs the state sequences after the BUTTON press between two builds.
"""

import argparse
import asyncio
import pathlib
import sys
import time

from aioesphomeapi import APIClient, APIConnectionError, ButtonInfo


async def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("host")
    parser.add_argument("button", choices=("Energy steps", "Simple steps"))
    parser.add_argument("out")
    args = parser.parse_args()
    client = APIClient(args.host, 6053, None)
    try:
        await asyncio.wait_for(client.connect(login=True), 20)
    except (APIConnectionError, OSError, TimeoutError) as err:
        print(f"no API connection: {err!r}", file=sys.stderr)
        sys.exit(3)
    entities, _ = await client.list_entities_services()
    names = {e.key: e.name for e in entities}
    buttons = {e.name: e.key for e in entities if isinstance(e, ButtonInfo)}
    out: list[str] = []
    t0 = time.monotonic()

    def stamp() -> str:
        return f"{time.monotonic() - t0:7.2f}"

    client.subscribe_states(
        lambda s: out.append(
            f"{stamp()} STATE {names.get(s.key, s.key)} = {getattr(s, 'state', None)!r}"
        )
    )
    client.subscribe_logs(
        lambda m: out.append(f"{stamp()} LOG {m.message.decode(errors='replace')}"),
        log_level=5,
        dump_config=True,
    )
    await asyncio.sleep(3)
    for name, wait in (("Sim stats", 1), (args.button, 14)):
        out.append(f"{stamp()} PRESS {name}")
        client.button_command(buttons[name])
        await asyncio.sleep(wait)
    await client.disconnect()
    pathlib.Path(args.out).write_text("\n".join(out) + "\n", encoding="utf-8")


asyncio.run(main())
