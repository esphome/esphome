"""improvscan.py PORT OUT: send an Improv RPC 'request scan' and dump each reply's strings (auth field raw bytes)."""

import pathlib
import sys
import time

import serial

P, OUT = sys.argv[1], sys.argv[2]
s = serial.Serial()
s.port = P
s.baudrate = 115200
s.timeout = 0.2
s.dtr = False
s.rts = False
s.open()
time.sleep(30.0)
s.reset_input_buffer()


def pkt(t, data):
    b = b"IMPROV" + bytes([1, t, len(data)]) + data
    return b + bytes([sum(b) & 0xFF])


s.write(pkt(0x03, bytes([0x04, 0x00])))
s.flush()
buf = b""
end = time.time() + 15
while time.time() < end:
    buf += s.read(4096)
s.close()
lines = []
i = 0
n = 0
while (i := buf.find(b"IMPROV", i)) >= 0:
    if i + 9 > len(buf):
        break
    ver, typ, ln = buf[i + 6], buf[i + 7], buf[i + 8]
    body = buf[i + 9 : i + 9 + ln]
    ck = buf[i + 9 + ln] if i + 9 + ln < len(buf) else None
    ok = ck == (sum(buf[i : i + 9 + ln]) & 0xFF)
    if typ == 0x04:
        cmd, tl = body[0], body[1]
        strs = []
        j = 2
        while j < 2 + tl:
            slen = body[j]
            strs.append(body[j + 1 : j + 1 + slen])
            j += 1 + slen
        n += 1
        if strs:
            lines.append(
                f"reply cmd={cmd} checksum_ok={ok} fields={len(strs)} auth={strs[-1]!r} auth_len={len(strs[-1])} rssi_len={len(strs[1]) if len(strs) > 1 else -1}"
            )
        else:
            lines.append(f"reply cmd={cmd} checksum_ok={ok} end of list")
    i += 9 + ln
pathlib.Path(OUT).write_text("\n".join(lines) + "\n", encoding="utf-8")
print(
    f"{n} replies; auth values: {sorted({line.split('auth=')[1].split()[0] for line in lines if 'auth=' in line})}; bad checksums: {sum('checksum_ok=False' in line for line in lines)}"
)
