"""press.py HOST [BUTTON]: press a button (default Restart) over the API. env: API_KEY"""

import asyncio
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect  # noqa: E402


async def main(host: str, name: str) -> None:
    cli = await connect(host)
    ents, _ = await cli.list_entities_services()
    button = next(e for e in ents if e.name == name)
    cli.button_command(button.key)
    await asyncio.sleep(1)
    await cli.disconnect()


asyncio.run(main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "Restart"))
