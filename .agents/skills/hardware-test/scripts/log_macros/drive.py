"""drive.py HOST OUT: press exercise, read API sensor states and web_server sensor JSON."""

import asyncio
import http.client
import pathlib
import sys
import urllib.parse
import urllib.request

from aioesphomeapi import APIClient, ButtonInfo, SensorInfo

HOST, OUT = sys.argv[1], sys.argv[2]


async def main():
    c = APIClient(HOST, 6053, None)
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    b = {e.name: e for e in ents if isinstance(e, ButtonInfo)}
    sens = {e.key: e for e in ents if isinstance(e, SensorInfo)}
    st = {}
    c.subscribe_states(lambda s: st.__setitem__(s.key, s) if s.key in sens else None)
    await asyncio.sleep(7)
    c.button_command(b["exercise"].key)
    await asyncio.sleep(3)
    out = []
    for k, e in sorted(sens.items(), key=lambda x: x[1].name):
        s = st.get(k)
        out.append(
            f"API {e.name}: state={s.state!r} missing={s.missing_state} acc={e.accuracy_decimals}"
            if s
            else f"API {e.name}: no state"
        )
        for path in (f"/sensor/{urllib.parse.quote(e.name)}", f"/sensor/{e.object_id}"):
            try:
                out.append(
                    f"WEB {e.name}: "
                    + urllib.request.urlopen(f"http://{HOST}{path}", timeout=5)
                    .read()
                    .decode()
                )
                break
            except (OSError, http.client.HTTPException) as ex:
                last = ex
        else:
            out.append(f"WEB {e.name}: error {last}")
    await c.disconnect()
    pathlib.Path(OUT).write_text("\n".join(out) + "\n", encoding="utf-8")


asyncio.run(main())
