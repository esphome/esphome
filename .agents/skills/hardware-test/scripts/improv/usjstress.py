"""usjstress.py PORT OUT SECONDS: capture the console while writing random bytes and text lines into it.

Writes start after 10 s: 20 chunks of 200 B (alternating text lines and random bytes) every 0.25 s, then a 2 KB
blob, repeated every 15 s until 10 s before the end. Uses write_timeout and no flush, since nothing on the device
may read the port. Prints bytes accepted, timed-out writes, chunks dropped while the port was gone and read errors.
Opening a C3/S3 USB Serial/JTAG port resets the chip once, which is why the writes wait 10 s. A port that drops is
reopened, also under a new path, and marked `#READ ERROR` and `#REOPENED` in OUT.
"""

import contextlib
import os
import pathlib
import sys
import threading
import time
from types import SimpleNamespace

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import serial  # noqa: E402
from serial_capture import PORT_ERRORS, current_path, identify, open_port  # noqa: E402


def main() -> None:
    port, out, secs = sys.argv[1], sys.argv[2], float(sys.argv[3])
    opts = SimpleNamespace(baud=115200, released=True, reset=False)
    serial_number, index = identify(port)
    link = SimpleNamespace(ser=open_port(opts, port), read_errors=0)
    link.ser.write_timeout = 0.5
    end = time.time() + secs
    with pathlib.Path(out).open("wb") as f:

        def reader() -> None:
            while time.time() < end:
                if link.ser is None:
                    try:
                        s = open_port(opts, current_path(port, serial_number, index))
                        s.write_timeout = 0.5
                        link.ser = s
                        f.write(f"\n#REOPENED {s.port}\n".encode())
                    except PORT_ERRORS:
                        time.sleep(0.5)
                    continue
                try:
                    d = link.ser.read(4096)
                except PORT_ERRORS as e:
                    f.write(f"\n#READ ERROR {e}\n".encode())
                    link.read_errors += 1
                    with contextlib.suppress(*PORT_ERRORS):
                        link.ser.close()
                    link.ser = None
                    continue
                if d:
                    f.write(d)

        t = threading.Thread(target=reader)
        t.start()
        time.sleep(10)
        acc = tout = rounds = dropped = 0
        while time.time() < end - 10:
            rounds += 1
            t_round = time.time()
            chunks = [
                os.urandom(200)
                if i % 2
                else (f"host text line {rounds}.{i} " + "x" * 160 + "\r\n").encode()
                for i in range(20)
            ]
            chunks.append(os.urandom(2048))
            for c in chunks:
                try:
                    if (s := link.ser) is None:
                        dropped += 1
                    else:
                        acc += s.write(c)
                except serial.SerialTimeoutException:
                    tout += 1
                except PORT_ERRORS:
                    dropped += 1  # the reader reopens the port
                time.sleep(0.25)
            time.sleep(max(0, min(t_round + 15, end - 10) - time.time()))
        t.join()
        if link.ser is not None:
            link.ser.close()
    print(
        f"rounds={rounds} bytes_accepted={acc} write_timeouts={tout} dropped_chunks={dropped} "
        f"read_errors={link.read_errors}"
    )


if __name__ == "__main__":
    main()
