"""Print the date, time and datetime rows of /metrics, set new values over the API, print them again.

usage: datetime_metrics.py HOST   env: API_KEY
Sets every date to 2040-03-04, every time to 23:59:59 and every datetime to 2040-03-04 05:06:07 UTC (past 2038 on
purpose). Values are printed with their UTC reading: dates are local midnight, times are ms since midnight and
datetimes are ms since the epoch. Exits 1 when a time or datetime row does not hold the value that was set.
"""

import asyncio
from datetime import UTC, datetime
import os
import re
import sys
import urllib.request

from aioesphomeapi import APIClient, DateInfo, DateTimeInfo, TimeInfo

HOST = sys.argv[1]


def metrics(tag: str) -> dict[tuple[str, str], int]:
    with urllib.request.urlopen(f"http://{HOST}/metrics", timeout=10) as resp:  # noqa: S310
        body = resp.read().decode()
    rows = [
        line
        for line in body.splitlines()
        if re.match(r"esphome_(date|time|datetime)_", line)
    ]
    print(f"--- {tag}: {len(rows)} rows")
    parsed: dict[tuple[str, str], int] = {}
    for line in rows:
        m = re.match(r'(esphome_\w+)\{.*name="([^"]+)"\} (-?\d+)$', line)
        if not m:
            print(f"  UNPARSED {line}")
            continue
        metric, name, value = m.group(1), m.group(2), int(m.group(3))
        parsed[(metric, name)] = value
        utc = ""
        if metric in ("esphome_date_value", "esphome_datetime_value"):
            utc = " = " + datetime.fromtimestamp(value / 1000, UTC).isoformat()
        print(f"  {metric} {name} {value}{utc}")
    return parsed


async def main() -> None:
    if not metrics("initial"):
        sys.exit(
            "NO-DATA: no date, time or datetime rows (a build without them, or no such entities)"
        )
    c = APIClient(HOST, 6053, None, noise_psk=os.environ.get("API_KEY"))
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    sent = 0
    for e in ents:
        sent += isinstance(e, (DateInfo, TimeInfo, DateTimeInfo))
        if isinstance(e, DateInfo):
            c.date_command(e.key, 2040, 3, 4)
        elif isinstance(e, TimeInfo):
            c.time_command(e.key, 23, 59, 59)
        elif isinstance(e, DateTimeInfo):
            c.datetime_command(
                e.key, int(datetime(2040, 3, 4, 5, 6, 7, tzinfo=UTC).timestamp())
            )
    await asyncio.sleep(2)
    await c.disconnect()
    if not sent:
        sys.exit("NO-DATA: the device lists no date, time or datetime entities")
    after = metrics("after setting 2040-03-04, 23:59:59 and 2040-03-04 05:06:07 UTC")
    if not after:
        sys.exit("NO-DATA: no rows after setting the values")
    # dates are local midnight, so their value depends on the device's time zone and is only printed
    want = {"esphome_time_value": 86399000, "esphome_datetime_value": 2214450367000}
    wrong = [
        f"{metric} {name} {value} (want {want[metric]})"
        for (metric, name), value in after.items()
        if metric in want and value != want[metric]
    ]
    for line in wrong:
        print(f"  WRONG {line}")
    sys.exit(1 if wrong else 0)


asyncio.run(main())
