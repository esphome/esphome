"""clistate.py HOST [SECS]: print the Madoka climate state as published over the API."""

import asyncio
import sys

from aioesphomeapi import APIClient, ClimateInfo


async def main() -> int:
    c = APIClient(sys.argv[1], 6053, None)
    await c.connect(login=True)
    infos, _ = await c.list_entities_services()
    key = next(i.key for i in infos if isinstance(i, ClimateInfo))
    seen = []
    c.subscribe_states(lambda s: seen.append(s) if s.key == key else None)
    await asyncio.sleep(float(sys.argv[2]) if len(sys.argv) > 2 else 3)
    for s in seen[-1:]:
        fan = s.fan_mode.name if s.fan_mode is not None else None
        print(
            f"mode={s.mode.name} low={s.target_temperature_low} high={s.target_temperature_high}"
            f" target={s.target_temperature} current={s.current_temperature} fan={fan} action={s.action}"
        )
    print("updates seen:", len(seen))
    await c.disconnect()
    return 0 if seen else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
