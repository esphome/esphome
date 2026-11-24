"""Plaintext native API client that lists user actions with their raw argument fields and executes one.

usage: rawactions.py HOST list
       rawactions.py HOST exec ACTION key=value [...]   (every argument must be given; types from the listing)
Fields of ListEntitiesServicesArgument: 1 name, 2 type, 3 description, 4 example, 5 optional, 6 default_value.
"""

import os
import socket
import struct
import sys

HOST = sys.argv[1]
CMD = sys.argv[2]
ARGS = sys.argv[3:]
TYPE_NAMES = {
    0: "bool",
    1: "int",
    2: "float",
    3: "string",
    4: "bool[]",
    5: "int[]",
    6: "float[]",
    7: "string[]",
}


def varint(v: int) -> bytes:
    out = bytearray()
    while True:
        b = v & 0x7F
        v >>= 7
        if v:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def svarint(v: int) -> bytes:
    return varint((v << 1) ^ (v >> 63))


def decode_varint(buf: bytes, pos: int) -> tuple[int, int]:
    value = shift = 0
    while True:
        b = buf[pos]
        pos += 1
        value |= (b & 0x7F) << shift
        if not b & 0x80:
            return value, pos
        shift += 7


def fields(payload: bytes) -> list[tuple[int, int, object]]:
    out = []
    pos = 0
    while pos < len(payload):
        key, pos = decode_varint(payload, pos)
        fnum, wt = key >> 3, key & 7
        if wt == 0:
            v, pos = decode_varint(payload, pos)
        elif wt == 2:
            ln, pos = decode_varint(payload, pos)
            v = payload[pos : pos + ln]
            pos += ln
        elif wt == 5:
            v = payload[pos : pos + 4]  # fixed32, caller unpacks
            pos += 4
        else:
            raise ValueError(f"wire type {wt}")
        out.append((fnum, wt, v))
    return out


def frame(mtype: int, payload: bytes) -> bytes:
    return b"\x00" + varint(len(payload)) + varint(mtype) + payload


def field_bytes(fnum: int, data: bytes) -> bytes:
    return varint((fnum << 3) | 2) + varint(len(data)) + data


def field_varint(fnum: int, v: int) -> bytes:
    return varint((fnum << 3) | 0) + varint(v)


def recv_frames(
    sock: socket.socket, until: set[int], limit: float = 5.0
) -> list[tuple[int, bytes]]:
    sock.settimeout(limit)
    buf = bytearray()
    frames = []
    while True:
        try:
            chunk = sock.recv(4096)
        except TimeoutError:
            return frames
        if not chunk:
            return frames
        buf += chunk
        while True:
            if len(buf) < 3:
                break
            if buf[0] != 0:
                raise ValueError("not plaintext")
            try:
                ln, p = decode_varint(buf, 1)
                mt, p = decode_varint(buf, p)
            except IndexError:
                break
            if len(buf) < p + ln:
                break
            frames.append((mt, bytes(buf[p : p + ln])))
            del buf[: p + ln]
            if mt in until:
                return frames


def main() -> None:
    sock = socket.create_connection((HOST, 6053), timeout=5)
    sock.sendall(
        frame(
            1, field_bytes(1, b"rawactions") + field_varint(2, 1) + field_varint(3, 10)
        )
    )  # HelloRequest
    recv_frames(sock, {2})
    sock.sendall(frame(3, b""))  # ConnectRequest, no password
    recv_frames(sock, {4})
    sock.sendall(frame(11, b""))  # ListEntitiesRequest
    frames = recv_frames(sock, {19})
    services = {}
    for mt, payload in frames:
        if mt != 41:
            continue
        name = key = None
        args = []
        for fnum, _, v in fields(payload):
            if fnum == 1:
                name = v.decode()
            elif fnum == 2:
                key = struct.unpack("<I", v)[0]
            elif fnum == 3:
                a = {
                    "name": None,
                    "type": 0,
                    "description": None,
                    "example": None,
                    "optional": None,
                    "default": None,
                }
                for af, _, av in fields(v):
                    if af == 1:
                        a["name"] = av.decode()
                    elif af == 2:
                        a["type"] = av
                    elif af == 3:
                        a["description"] = av.decode()
                    elif af == 4:
                        a["example"] = av.decode()
                    elif af == 5:
                        a["optional"] = bool(av)
                    elif af == 6:
                        a["default"] = av.decode()
                    else:
                        a[f"field{af}"] = av
                args.append(a)
        services[name] = (key, args)
    if CMD == "list":
        for name, (key, args) in services.items():
            print(f"action {name} key={key}")
            for a in args:
                extra = {
                    k: v
                    for k, v in a.items()
                    if k not in ("name", "type") and v is not None
                }
                print(f"  {a['name']}: {TYPE_NAMES.get(a['type'], a['type'])} {extra}")
        return
    name = ARGS[0]
    key, args = services[name]
    payload = varint((1 << 3) | 5) + struct.pack("<I", key)
    given = dict(kv.split("=", 1) for kv in ARGS[1:])
    for a in args:
        if a["name"] not in given:
            if os.environ.get("ALLOW_MISSING"):
                continue  # leave the argument out of the request on purpose
            sys.exit(f"missing {a['name']}")
        raw = given[a["name"]]
        t = a["type"]
        if t == 0:
            av = field_varint(1, 1 if raw.lower() == "true" else 0)
        elif t == 1:
            av = varint((5 << 3) | 0) + svarint(int(raw))  # sint32 int_
        elif t == 2:
            av = varint((3 << 3) | 5) + struct.pack("<f", float(raw))
        elif t == 3:
            av = field_bytes(4, raw.encode())
        elif t == 5:
            av = b"".join(
                varint((7 << 3) | 0) + svarint(int(x)) for x in raw.split(",") if x
            )  # repeated sint32 int_array
        else:
            sys.exit(f"type {t} not supported here")
        payload += field_bytes(2, av)
    sock.sendall(frame(42, payload))  # ExecuteServiceRequest
    print(f"sent {name} with {given}")
    sock.close()


main()
