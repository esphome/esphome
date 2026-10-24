"""Drive the ota.http_request cases over the API.
Usage: hodrive.py IP BASEURL (BASEURL serves fw.bin, fw.md5, bad.md5, auth/fw.bin, auth/fw.md5)"""

import asyncio
import pathlib
import re
import sys
import time

import aioesphomeapi

IP, BASE = sys.argv[1:3]
SUM = (
    pathlib.Path(sys.argv[3]).read_text(encoding="utf-8").strip()
    if len(sys.argv) > 3
    else ""
)
T0 = time.monotonic()
lines: list[str] = []
failed: list[str] = []


def out(s: str) -> None:
    print(f"{time.monotonic() - T0:7.2f} {s}", flush=True)


async def connect() -> tuple[aioesphomeapi.APIClient, dict]:
    last: Exception | None = None
    for _ in range(90):
        c = aioesphomeapi.APIClient(IP, 6053, None)
        try:
            await c.connect(login=True)
        except (
            aioesphomeapi.InvalidAuthAPIError,
            aioesphomeapi.InvalidEncryptionKeyAPIError,
            aioesphomeapi.RequiresEncryptionAPIError,
        ):
            raise  # a key or password problem never fixes itself
        except Exception as exc:  # noqa: BLE001
            last = exc
            await asyncio.sleep(2)
            continue
        i = await c.device_info()
        out(f"connected {i.name} {i.compilation_time}")
        _, svcs = await c.list_entities_services()

        def on_log(m):
            t = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
            if re.search(r"\[hot|http_request|ota|OTA|MD5|md5", t):
                lines.append(t)
                out(t[:170])

        c.subscribe_logs(on_log, log_level=aioesphomeapi.LogLevel.LOG_LEVEL_DEBUG)
        return c, {x.name: x for x in svcs}
    raise SystemExit(f"device never came back (last error {last!r})")


async def case(c, s, name, args, want, timeout=60.0):
    out(f"=== {name} {args} (want {want})")
    start = len(lines)
    await c.execute_service(s[name], args)
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if any(re.search(want, x) for x in lines[start:]):
            out(f"RESULT {name} PASS")
            return True
        await asyncio.sleep(0.2)
    out(f"RESULT {name} FAIL (no '{want}')")
    failed.append(name)
    return False


async def flash_ok(c, s, name, args):
    """A good flash: wait for the device to drop and come back."""
    out(f"=== {name} {args} (want reboot)")
    start = len(lines)
    await c.execute_service(s[name], args)
    end = time.monotonic() + 120
    while (
        time.monotonic() < end
        and c._connection is not None
        and c._connection.is_connected
    ):  # noqa: SLF001
        await asyncio.sleep(0.5)
    # a good flash needs both the device's own success line and the connection dropping for the reboot
    dropped = c._connection is None or not c._connection.is_connected  # noqa: SLF001
    logged = any(
        re.search(r"(?i)update complete|rebooting|successful", x) for x in lines[start:]
    )
    if not dropped:
        out(f"{name}: the device never dropped the connection within 120 s")
    ok = logged and dropped
    await c.disconnect()
    await asyncio.sleep(5)
    c, s = await connect()
    out(f"RESULT {name} {'PASS' if ok else 'FAIL'}")
    if not ok:
        failed.append(name)
    return c, s


async def main():
    c, s = await connect()
    u, m = f"{BASE}/fw.bin", f"{BASE}/fw.md5"
    await case(c, s, "getters", {}, r"computed='' expected=''")
    await case(c, s, "fl_nourl", {}, r"URL not set")
    await case(c, s, "fl_badurl", {}, r"URL not set")
    await case(c, s, "getters", {}, r"computed='' expected=''")
    await case(c, s, "fl_md5", {"url": u, "sum": "0" * 32}, r"MD5 mismatch")
    await case(c, s, "getters", {}, r"getters computed=")
    await case(
        c,
        s,
        "fl_auth",
        {
            "url": f"{BASE}/auth/fw.bin",
            "md5url": f"{BASE}/auth/fw.md5",
            "user": "otauser",
            "pw": "wrong",
        },
        r"HTTP Request failed; URL: \S*/auth/fw\.md5; Code: 401",
    )
    c, s = await flash_ok(
        c,
        s,
        "fl_auth",
        {
            "url": f"{BASE}/auth/fw.bin",
            "md5url": f"{BASE}/auth/fw.md5",
            "user": "otauser",
            "pw": "otapass",
        },
    )
    c, s = await flash_ok(c, s, "fl_md5url", {"url": u, "md5url": m})
    c, s = await flash_ok(c, s, "fl_md5", {"url": u, "sum": SUM})
    await case(c, s, "getters", {}, r"computed='' expected=''")
    await c.disconnect()
    out(
        f"SUMMARY {'all cases passed' if not failed else 'FAILED: ' + ', '.join(failed)}"
    )
    return 1 if failed else 0


sys.exit(asyncio.run(main()))
