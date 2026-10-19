"""Play a URL on the speaker_source media player over the API (no sync task involved).

usage: playurl.py HOST KEY URL [PLAYER_NAME]
"""

import asyncio
import sys

from aioesphomeapi import APIClient, MediaPlayerInfo

HOST, KEY, URL = sys.argv[1], sys.argv[2], sys.argv[3]
PLAYER = sys.argv[4] if len(sys.argv) > 4 else "Player"


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=KEY)
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    players = {e.name: e for e in ents if isinstance(e, MediaPlayerInfo)}
    print("media players:", sorted(players))
    p = players[PLAYER]
    c.media_player_command(p.key, media_url=URL)
    print("sent play_media to", PLAYER)
    await asyncio.sleep(1)
    await c.disconnect()


asyncio.run(main())
