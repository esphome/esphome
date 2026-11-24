"""Drive the speaker platform media player (the first media player found) over one API connection.

usage: drive.py HOST KEY op [op ...]
ops: play:URL | ann:URL | stop | press:BUTTON | wait:S
"""

import asyncio
import sys

from aioesphomeapi import APIClient, ButtonInfo, MediaPlayerCommand, MediaPlayerInfo

HOST, KEY, OPS = sys.argv[1], sys.argv[2], sys.argv[3:]


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=None if KEY == "-" else KEY)
    await c.connect(login=True)
    try:
        await run_ops(c)
    finally:
        await c.disconnect()


async def run_ops(c: APIClient) -> None:
    ents, _ = await c.list_entities_services()
    player = next((e for e in ents if isinstance(e, MediaPlayerInfo)), None)
    if player is None:
        sys.exit("no media player on this device")
    buttons = {e.name: e for e in ents if isinstance(e, ButtonInfo)}
    for op in OPS:
        kind, _, rest = op.partition(":")
        print(f"{asyncio.get_running_loop().time():.1f} {op}", flush=True)
        if kind == "play":
            c.media_player_command(player.key, media_url=rest)
        elif kind == "ann":
            c.media_player_command(player.key, media_url=rest, announcement=True)
        elif kind == "stop":
            c.media_player_command(player.key, command=MediaPlayerCommand.STOP)
        elif kind == "press":
            if rest not in buttons:
                sys.exit(f"no button {rest!r}; have {sorted(buttons)}")
            c.button_command(buttons[rest].key)
        elif kind == "wait":
            await asyncio.sleep(float(rest))
        else:
            sys.exit(f"unknown op {op!r}")
    await asyncio.sleep(0.5)


asyncio.run(main())
