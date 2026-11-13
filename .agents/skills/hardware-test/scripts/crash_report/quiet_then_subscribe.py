"""quiet_then_subscribe.py HOST PORT OUT QUIET_BOOTS: on the first boot drain any crash record (the first log
subscribe replays and clears it), then stay connected without a log subscription for QUIET_BOOTS reboots, then
subscribe on the next boot and record the log (including a crash record replayed on subscribe) until a line with
REBOOT. env: API_KEY (unset for a plaintext API)."""

import asyncio
import contextlib
import pathlib
import sys
import time

from aioesphomeapi import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text, stop_event  # noqa: E402


async def main(host: str, port: int, outf: str, quiet: int) -> None:
    with pathlib.Path(outf).open("a", encoding="utf-8") as f:

        def w(t: str) -> None:
            f.write(f"{time.strftime('%H:%M:%S')} {t}\n")
            f.flush()

        boots = 0
        while True:
            lost, on_stop = stop_event()
            cli = await connect(host, tries=300, timeout=5, port=port, on_stop=on_stop)
            boots += 1
            info = await cli.device_info()
            w(
                f"=== CONNECTED boot {boots} name={info.name} quiet={1 < boots <= quiet + 1}"
            )
            if boots > quiet + 1:
                done = asyncio.Event()

                def on_log(m, done: asyncio.Event = done) -> None:
                    t = log_text(m)
                    w(t)
                    if "REBOOT" in t:
                        done.set()

                cli.subscribe_logs(
                    on_log, log_level=LogLevel.LOG_LEVEL_DEBUG, dump_config=False
                )
                try:
                    await asyncio.wait_for(done.wait(), 120)
                except TimeoutError:
                    w("=== no REBOOT within 120 s")
                with contextlib.suppress(Exception):
                    await cli.disconnect()
                return
            if boots == 1:
                # drain any crash record left by the reboot that brought this build up
                cli.subscribe_logs(
                    lambda m: w("drain " + log_text(m)),
                    log_level=LogLevel.LOG_LEVEL_DEBUG,
                    dump_config=False,
                )
                await asyncio.sleep(3)
            # quiet: wait for the device to drop the connection (its reboot)
            await lost.wait()
            w("=== connection lost")
            await asyncio.sleep(2)


asyncio.run(main(sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4])))
