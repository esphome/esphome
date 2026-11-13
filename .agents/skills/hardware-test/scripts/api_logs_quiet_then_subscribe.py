"""api_logs_quiet_then_subscribe.py HOST PORT OUT QUIET_BOOTS: stay connected without a log subscription for
QUIET_BOOTS boots, then subscribe on the next boot and record the log (including any crash record replayed on
subscribe) until a line with REBOOT. Plaintext API only."""

import asyncio
import pathlib
import sys
import time

from aioesphomeapi import APIClient, LogLevel

HOST, PORT, OUT, QUIET = sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4])


async def main() -> None:
    f = pathlib.Path(OUT).open("a", encoding="utf-8")  # noqa: SIM115

    def w(t):
        f.write(f"{time.strftime('%H:%M:%S')} {t}\n")
        f.flush()

    boots = 0
    while True:
        cli = APIClient(HOST, PORT, None)
        try:
            await asyncio.wait_for(cli.connect(login=True), 5)
        except Exception:  # noqa: BLE001
            await asyncio.sleep(0.3)
            continue
        boots += 1
        info = await cli.device_info()
        w(f"=== CONNECTED boot {boots} name={info.name} quiet={1 < boots <= QUIET + 1}")
        done = asyncio.Event()
        if boots > QUIET + 1:

            def on_log(m, done: asyncio.Event = done) -> None:
                t = m.message.decode(errors="replace")
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
            f.close()
            return
        if boots == 1:
            # drain any crash record left by the reboot that brought this build up
            cli.subscribe_logs(
                lambda m: w("drain " + m.message.decode(errors="replace")),
                log_level=LogLevel.LOG_LEVEL_DEBUG,
                dump_config=False,
            )
            await asyncio.sleep(3)
        # quiet: wait for the device to drop the connection (its reboot)
        lost = asyncio.Event()
        await cli.disconnect()
        cli = APIClient(HOST, PORT, None)

        async def on_stop(expected: bool, lost: asyncio.Event = lost) -> None:
            lost.set()

        await cli.connect(on_stop=on_stop, login=True)
        await lost.wait()
        w("=== connection lost")
        await asyncio.sleep(2)


asyncio.run(main())
