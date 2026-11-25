"""apilogcap.py HOST SECONDS OUT: DEBUG log lines over the API, stamped with the local receive time like
serial_capture.py --ms, so devtime.py works on a proxy with no USB console; reconnects until the deadline and
writes APILOGCAP SESSION LOST when a session ends and APILOGCAP CONNECT FAILED when a reconnect fails."""

import asyncio
import contextlib
import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from aioesphomeapi import LogLevel  # noqa: E402
from apiconn import connect, log_text, stop_event  # noqa: E402


async def main() -> None:
    host, secs, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
    end = time.time() + secs
    with pathlib.Path(out).open("a", encoding="utf-8") as f:

        def on_log(m) -> None:
            t = time.time()
            stamp = (
                time.strftime("[%H:%M:%S", time.localtime(t))
                + f".{int((t % 1) * 1000):03d}]"
            )
            f.write(stamp + log_text(m) + "\n")
            f.flush()

        while time.time() < end:
            ev, on_stop = stop_event()
            try:
                cli = await connect(host, tries=5, on_stop=on_stop)
            except SystemExit as err:
                # Keep the gap visible: devtime.py skips the line, a reader sees the outage.
                print(f"apilogcap: connect failed ({err}), retrying", file=sys.stderr)
                f.write(time.strftime("[%H:%M:%S.000]") + " APILOGCAP CONNECT FAILED\n")
                f.flush()
                await asyncio.sleep(5)
                continue
            cli.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
            with contextlib.suppress(TimeoutError):
                await asyncio.wait_for(ev.wait(), max(1, end - time.time()))
            if ev.is_set():
                # a quick reboot reconnects at the first try: mark every lost session, not only failed connects
                f.write(time.strftime("[%H:%M:%S.000]") + " APILOGCAP SESSION LOST\n")
                f.flush()
            with contextlib.suppress(Exception):
                await cli.disconnect()


asyncio.run(main())
