"""List every entity over the native API with its icon, device class and unit: one sorted JSON line each.

usage: api_list.py HOST KEY OUT
"""

import asyncio
import json
import pathlib
import sys

from aioesphomeapi import APIClient

HOST, KEY, OUT = sys.argv[1], sys.argv[2], sys.argv[3]


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=KEY)
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    rows = []
    for e in ents:
        row = {
            "type": type(e).__name__,
            "name": e.name,
            "object_id": e.object_id,
            "icon": e.icon,
        }
        for k in (
            "device_class",
            "unit_of_measurement",
            "event_types",
            "options",
            "mode",
        ):
            if hasattr(e, k):
                v = getattr(e, k)
                row[k] = (
                    list(v)
                    if isinstance(v, (list, tuple))
                    else (v.name if hasattr(v, "name") else v)
                )
        row["entity_category"] = (
            e.entity_category.name
            if hasattr(e.entity_category, "name")
            else e.entity_category
        )
        row["disabled_by_default"] = e.disabled_by_default
        rows.append(row)
    rows.sort(key=lambda r: (r["type"], r["name"]))
    with pathlib.Path(OUT).open("w", encoding="utf-8") as f:
        f.writelines(json.dumps(r, sort_keys=True) + "\n" for r in rows)
    print(f"{len(rows)} entities listed")
    await c.disconnect()


asyncio.run(main())
