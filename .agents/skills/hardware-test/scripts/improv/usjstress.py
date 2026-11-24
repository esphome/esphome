"""usjstress.py PORT OUT SECONDS: capture the console while writing random bytes and text lines into it.

Writes start after 10 s: 20 chunks of 200 B (alternating text lines and random bytes) every 0.25 s, then a 2 KB
blob, repeated every 15 s until 10 s before the end. Uses write_timeout and no flush, since nothing on the device
may read the port. Prints bytes accepted and timed-out writes. Opening a C3/S3 USB Serial/JTAG port resets the chip
once, which is why the writes wait 10 s.
"""

import os
import pathlib
import sys
import threading
import time

import serial


def main() -> None:
    port, out, secs = sys.argv[1], sys.argv[2], float(sys.argv[3])
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout, ser.write_timeout = port, 115200, 0.05, 0.5
    ser.dtr = ser.rts = False
    ser.open()
    end = time.time() + secs
    stop = threading.Event()
    with pathlib.Path(out).open("wb") as f:

        def reader() -> None:
            while not stop.is_set() and time.time() < end:
                try:
                    d = ser.read(4096)
                except serial.SerialException as e:
                    f.write(f"\n#READ ERROR {e}\n".encode())
                    time.sleep(0.5)
                    continue
                if d:
                    f.write(d)

        t = threading.Thread(target=reader)
        t.start()
        time.sleep(10)
        acc = tout = rounds = 0
        while time.time() < end - 10:
            rounds += 1
            chunks = [
                os.urandom(200)
                if i % 2
                else (f"host text line {rounds}.{i} " + "x" * 160 + "\r\n").encode()[
                    :200
                ]
                for i in range(20)
            ]
            chunks.append(os.urandom(2048))
            for c in chunks:
                try:
                    acc += ser.write(c)
                except serial.SerialTimeoutException:
                    tout += 1
                time.sleep(0.25)
            time.sleep(max(0, min(15 - 5.25, end - 10 - time.time())))
        while time.time() < end:
            time.sleep(0.5)
        stop.set()
        t.join()
    print(f"rounds={rounds} bytes_accepted={acc} write_timeouts={tout}")


if __name__ == "__main__":
    main()
