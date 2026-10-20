"""Drive the speaker_source media player over one API connection.

usage: drive.py HOST KEY op [op ...]
ops: play:URL | ann:URL | stop | press:BUTTON | wait:S
"""

import asyncio
import sys

from aioesphomeapi import APIClient, ButtonInfo, MediaPlayerInfo

HOST, KEY, OPS = sys.argv[1], sys.argv[2], sys.argv[3:]


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=None if KEY == "-" else KEY)
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    player = next(e for e in ents if isinstance(e, MediaPlayerInfo))
    buttons = {e.name: e for e in ents if isinstance(e, ButtonInfo)}
    for op in OPS:
        kind, _, rest = op.partition(":")
        print(f"{asyncio.get_running_loop().time():.1f} {op}", flush=True)
        if kind == "play":
            c.media_player_command(player.key, media_url=rest)
        elif kind == "ann":
            c.media_player_command(player.key, media_url=rest, announcement=True)
        elif kind == "stop":
            from aioesphomeapi import MediaPlayerCommand

            c.media_player_command(player.key, command=MediaPlayerCommand.STOP)
        elif kind == "press":
            c.button_command(buttons[rest].key)
        elif kind == "wait":
            await asyncio.sleep(float(rest))
        else:
            sys.exit(f"unknown op {op!r}")
    await asyncio.sleep(0.5)
    await c.disconnect()


asyncio.run(main())
