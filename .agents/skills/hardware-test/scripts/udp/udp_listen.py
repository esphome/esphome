"""Log every datagram on PORT for SECONDS as one JSON line: time, source, length, hex payload."""

import json
import pathlib
import socket
import sys
import time

port, secs, out = int(sys.argv[1]), float(sys.argv[2]), sys.argv[3]
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("", port))
s.settimeout(1.0)
end = time.monotonic() + secs
with pathlib.Path(out).open("w", encoding="utf-8") as f:
    while time.monotonic() < end:
        try:
            data, (ip, sport) = s.recvfrom(65535)
        except TimeoutError:
            continue
        f.write(
            json.dumps(
                {
                    "t": time.strftime("%H:%M:%S"),
                    "src": ip,
                    "len": len(data),
                    "hex": data.hex(),
                }
            )
            + "\n"
        )
        f.flush()
