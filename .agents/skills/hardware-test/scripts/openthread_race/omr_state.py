"""omr_state.py HOST KEY [SECS]: print the device's "OMR Prefix" text sensor (omr.yaml) once it holds a prefix.

The value comes from the device over a new API connection, so it cannot be a record cached from before a restart.
Exits 1 when the sensor is missing or holds no prefix within SECS (default 15). KEY of - means a plaintext API.
"""

import asyncio
import sys

from aioesphomeapi import APIClient, TextSensorInfo

HOST, KEY = sys.argv[1], sys.argv[2]
SECS = float(sys.argv[3]) if len(sys.argv) > 3 else 15


async def main() -> int:
    c = APIClient(HOST, 6053, None, noise_psk=None if KEY == "-" else KEY)
    await c.connect(login=True)
    try:
        ents, _ = await c.list_entities_services()
        key = next(
            (
                e.key
                for e in ents
                if isinstance(e, TextSensorInfo) and e.name == "OMR Prefix"
            ),
            None,
        )
        if key is None:
            print("no OMR Prefix text sensor (include omr.yaml)", file=sys.stderr)
            return 1
        got = asyncio.Event()
        val = [""]

        def on_state(s) -> None:
            if s.key == key and not s.missing_state and s.state not in ("", "none"):
                val[0] = s.state
                got.set()

        c.subscribe_states(on_state)
        try:
            await asyncio.wait_for(got.wait(), SECS)
        except TimeoutError:
            print("omr=none")
            return 1
        print(f"omr={val[0]}")
        return 0
    finally:
        await c.disconnect()


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
