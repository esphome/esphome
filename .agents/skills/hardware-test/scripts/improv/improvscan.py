"""Improv RPC request scan over the logger UART: dump each reply's fields (auth field as raw bytes).

usage: improvscan.py PORT OUT [--settle S]
Opens the port with DTR and RTS low, waits SETTLE seconds (a D1 mini and a classic ESP32 still reset on open), sends
request scan (0x04) and reads until the end-of-list reply or 15 s. OUT gets one line per reply and OUT.raw the raw
stream; prints the reply count, the auth values seen and the bad checksum count.
"""

import argparse
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from improvcli import Dev, frame, rpc_strings, strings  # noqa: E402


class ScanDev(Dev):
    """Collects scan replies as they are parsed instead of printing every frame."""

    def __init__(self, *args) -> None:
        super().__init__(*args)
        self.lines: list[str] = []
        self.auths: set[bytes] = set()
        self.bad_checksums = 0

    def show(self, typ: int, data: bytes, ok: bool, short: bool) -> None:
        if typ != 4:
            return
        self.bad_checksums += not ok
        if short:
            self.lines.append(f"reply malformed {data.hex()} checksum_ok={ok}")
            return
        strs = strings(rpc_strings(data), raw=True)
        if not strs:
            self.lines.append(f"reply cmd={data[0]} checksum_ok={ok} end of list")
            return
        self.auths.add(strs[-1])
        self.lines.append(
            f"reply cmd={data[0]} checksum_ok={ok} fields={len(strs)} auth={strs[-1]!r} auth_len={len(strs[-1])} "
            f"rssi_len={len(strs[1]) if len(strs) > 1 else -1}"
        )


def main() -> None:
    a = argparse.ArgumentParser()
    a.add_argument("port")
    a.add_argument("out")
    a.add_argument("--settle", type=float, default=30)
    o = a.parse_args()
    d = ScanDev(o.port, 115200, o.out + ".raw", False, "")
    d.pump(o.settle)
    start = len(d.events)
    d.ser.write(frame(4))
    d.ser.flush()
    d.wait_for(
        lambda e: e[1] == 4 and e[3] and e[2][0] == 4 and e[2][1] == 0, 15, start
    )
    d.ser.close()
    pathlib.Path(o.out).write_text("\n".join(d.lines) + "\n", encoding="utf-8")
    print(
        f"{len(d.lines)} replies; auth values: {sorted(d.auths)}; bad checksums: {d.bad_checksums}"
    )


if __name__ == "__main__":
    main()
