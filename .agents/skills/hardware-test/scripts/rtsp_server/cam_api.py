"""Request one camera image over the native API and report its size (direct esp32_camera check)."""

import asyncio
import sys

from aioesphomeapi import APIClient


async def main():
    c = APIClient(sys.argv[1], 6053, None)
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    cams = [e for e in ents if type(e).__name__ == "CameraInfo"]
    print("camera entities:", [e.name for e in cams])
    got = asyncio.get_event_loop().create_future()

    def on_state(state):
        if type(state).__name__ == "CameraState" and not got.done():
            got.set_result(len(state.data))

    c.subscribe_states(on_state)
    c.request_single_image()
    try:
        n = await asyncio.wait_for(got, 10)
        print("single image bytes:", n)
    except TimeoutError:
        print("no image within 10 s")
    await c.disconnect()


asyncio.run(main())
