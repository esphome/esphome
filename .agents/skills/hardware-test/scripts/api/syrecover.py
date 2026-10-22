"""Pull the store_yaml blob off a device over the native API (plaintext or Noise) and unpack it.

usage: syrecover.py HOST KEY OUTDIR [ORIGDIR]   (KEY of - means a plaintext API)
Prints the capability flag, chunk count, advertised and received sizes, encoding, the envelope's files, and
when ORIGDIR is given compares each recovered file with ORIGDIR/<path>.
Run with PYTHONPATH pointing at the esphome tree that has store_yaml (for unpack_envelope). Needs Python 3.14
(compression.zstd) or the zstandard package.
"""

import asyncio
from pathlib import Path
import sys
import time

from aioesphomeapi import api_pb2
from aioesphomeapi._frame_helper.noise import APINoiseFrameHelper
from aioesphomeapi._frame_helper.plain_text import APIPlaintextFrameHelper
from aioesphomeapi.connection import APIConnection, ConnectionParams
from aioesphomeapi.zeroconf import ZeroconfManager

from esphome.components.store_yaml import unpack_envelope

try:
    from compression import zstd
except ImportError:
    try:
        import zstandard as zstd
    except ImportError:
        sys.exit("needs Python 3.14 (compression.zstd) or the zstandard package")

HOST, KEY, OUT = sys.argv[1], sys.argv[2], Path(sys.argv[3])
ORIG = Path(sys.argv[4]) if len(sys.argv) > 4 else None
HELLO, HELLO_RESP, CONNECT, CONNECT_RESP = 1, 2, 3, 4
CAPS_REQ, CAPS_RESP = 149, 150
GET_YAML_REQ, GET_YAML_RESP = 161, 162


def decode_varint(buf: bytes, pos: int) -> tuple[int, int]:
    value = shift = 0
    while True:
        b = buf[pos]
        pos += 1
        value |= (b & 0x7F) << shift
        if not b & 0x80:
            return value, pos
        shift += 7


def fields(payload: bytes) -> dict[int, list]:
    out: dict[int, list] = {}
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
            v = payload[pos : pos + 4]
            pos += 4
        else:
            raise ValueError(f"wire type {wt}")
        out.setdefault(fnum, []).append(v)
    return out


async def main() -> None:
    loop = asyncio.get_running_loop()
    queue: asyncio.Queue[tuple[int, bytes]] = asyncio.Queue()
    errors: list[Exception] = []

    class Shim(APIConnection):
        """Only the two callbacks the frame helper uses; the real connection logic is bypassed."""

        def process_packet(self, msg_type_proto, data) -> None:
            queue.put_nowait((msg_type_proto, bytes(data)))

        def report_fatal_error(self, err: Exception) -> None:
            errors.append(err)

    params = ConnectionParams(
        addresses=[HOST],
        port=6053,
        password=None,
        client_info="syrecover",
        keepalive=15,
        zeroconf_manager=ZeroconfManager(),
        noise_psk=None if KEY == "-" else KEY,
        expected_name=None,
        expected_mac=None,
        timezone=None,
        provide_time=True,
    )
    shim = Shim(params, None, False, HOST)
    if KEY == "-":
        helper = APIPlaintextFrameHelper(
            connection=shim, client_info="syrecover", log_name=HOST
        )
    else:
        helper = APINoiseFrameHelper(
            connection=shim,
            noise_psk=KEY,
            expected_name=None,
            expected_mac=None,
            client_info="syrecover",
            log_name=HOST,
        )
    await loop.create_connection(lambda: helper, HOST, 6053)
    await asyncio.wait_for(helper.ready_future, 10)
    print(f"transport {'plaintext' if KEY == '-' else 'noise'}: handshake done")

    async def expect(msg_type: int, timeout: float = 10) -> bytes:
        while True:
            t, d = await asyncio.wait_for(queue.get(), timeout)
            if t == msg_type:
                return d

    hello = api_pb2.HelloRequest(
        client_info="syrecover", api_version_major=1, api_version_minor=10
    )
    helper.write_packets([(HELLO, hello.SerializeToString())], False)
    await expect(HELLO_RESP)
    # Sent like the client library does; a device without a password never answers it
    helper.write_packets(
        [(CONNECT, api_pb2.AuthenticationRequest().SerializeToString())], False
    )
    helper.write_packets(
        [(CAPS_REQ, api_pb2.DeviceCapabilitiesRequest().SerializeToString())], False
    )
    caps = fields(await expect(CAPS_RESP))
    supported = None
    if 7 in caps:
        supported = fields(caps[7][0]).get(1) == [1]
    print(f"capabilities: store_yaml field present={7 in caps} supported={supported}")

    t0 = time.monotonic()
    helper.write_packets([(GET_YAML_REQ, b"")], False)
    chunks: list[bytes] = []
    total = encoding = None
    done = False
    while not done:
        f = fields(await expect(GET_YAML_RESP, 20))
        if 1 in f:
            chunks.append(f[1][0])
        done = f.get(2) == [1]
        if 3 in f:
            total = f[3][0]
        if 4 in f:
            encoding = f[4][0].decode()
    dt = time.monotonic() - t0
    compressed = b"".join(chunks)
    print(f"chunks {len(chunks)} sizes {[len(c) for c in chunks]}")
    print(
        f"received {len(compressed)} bytes, advertised {total}, encoding {encoding!r}, {dt:.2f} s"
    )
    # a second request right after: the device must answer again, not stay stuck
    helper.write_packets([(GET_YAML_REQ, b"")], False)
    again = 0
    done = False
    while not done:
        f = fields(await expect(GET_YAML_RESP, 20))
        again += len(f.get(1, [b""])[0])
        done = f.get(2) == [1]
    print(f"second request: {again} bytes")
    helper.close()
    if errors:
        print("transport errors:", errors)
    envelope = zstd.decompress(compressed)
    files = unpack_envelope(envelope)
    print(
        f"envelope {len(envelope)} bytes, {len(files)} files, ratio {len(compressed) / len(envelope):.2f}"
    )
    OUT.mkdir(parents=True, exist_ok=True)
    for rel, content in files.items():
        target = OUT / rel.replace("../", "up_/")
        if not target.resolve().is_relative_to(OUT.resolve()):
            print("skipping a path outside the output directory:", rel)
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
        note = ""
        if ORIG is not None:
            orig = (ORIG / rel).resolve()
            if orig.exists():
                note = (
                    "IDENTICAL to original"
                    if orig.read_bytes() == content
                    else f"DIFFERS from original ({orig.stat().st_size} bytes)"
                )
            else:
                note = "no original"
        print(f"  {rel}: {len(content)} bytes {note}")


asyncio.run(main())
