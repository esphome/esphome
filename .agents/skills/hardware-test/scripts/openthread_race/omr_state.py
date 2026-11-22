"""omr_state.py HOST KEY [SECS]: print the device's "OMR Prefix" text sensor and "Uptime" sensor (omr.yaml) as
"omr=PREFIX uptime=SECONDS" once it holds a prefix.

The values come from the device over a new API connection, so they cannot be a record cached from before a restart.
Exits 1 when a sensor is missing or no prefix comes within SECS (default 15). KEY of - means a plaintext API.
"""

import asyncio
import sys

from aioesphomeapi import APIClient, SensorInfo, TextSensorInfo

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
        ukey = next(
            (e.key for e in ents if isinstance(e, SensorInfo) and e.name == "Uptime"),
            None,
        )
        if key is None or ukey is None:
            print("no OMR Prefix or Uptime sensor (include omr.yaml)", file=sys.stderr)
            return 1
        got = asyncio.Event()
        val, up, err = [""], [None], [""]

        def on_state(s) -> None:
            if s.missing_state:
                return
            if s.key == key and s.state.startswith("error:"):
                err[0] = s.state
            elif s.key == key and s.state not in ("", "none"):
                val[0] = s.state
            elif s.key == ukey:
                up[0] = s.state
            if val[0] and up[0] is not None:
                got.set()

        c.subscribe_states(on_state)
        try:
            await asyncio.wait_for(got.wait(), SECS)
        except TimeoutError:
            print(f"omr={val[0] or err[0] or 'none'} uptime={up[0]}")
            return 1
        print(f"omr={val[0]} uptime={up[0]:.0f}")
        return 0
    finally:
        await c.disconnect()


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
