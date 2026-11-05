"""HTTP server with one misbehaviour per path, for the online_image stalled-download test."""

import asyncio
from pathlib import Path
import sys
import time

WWW = Path(sys.argv[1]).resolve()
NOT_FOUND = b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"


def www_file(rel: str) -> Path | None:
    """The file under WWW named by a request path, or None if missing or outside WWW."""
    file = (WWW / rel).resolve()
    return file if file.is_relative_to(WWW) and file.is_file() else None


PORT = int(sys.argv[2])
LOG = Path(sys.argv[3]).open("a", encoding="utf-8", buffering=1)  # noqa: SIM115

MIME = {"bmp": "image/bmp", "png": "image/png", "jpg": "image/jpeg", "qoi": "image/qoi"}
IMG = {n: (WWW / f"ok.{n}").read_bytes() for n in MIME}
BMP = IMG["bmp"]
BMP_HDR = 54  # 16x16 24-bit: pixel rows follow directly
LARGE_HEADER = (WWW / "large_header.bmp").read_bytes()


def log(msg: str) -> None:
    LOG.write(f"{time.strftime('%H:%M:%S')} {msg}\n")


def headers(ctype: str, extra: list[bytes]) -> bytes:
    lines = [
        b"HTTP/1.1 200 OK",
        b"Content-Type: " + ctype.encode(),
        b"Connection: close",
        *extra,
        b"",
        b"",
    ]
    return b"\r\n".join(lines)


def etag(path: str) -> bytes:
    return b'"' + path.strip("/").encode() + b'-v1"'


def chunked(body: bytes, size: int) -> bytes:
    out = b""
    for i in range(0, len(body), size):
        part = body[i : i + size]
        out += b"%x\r\n" % len(part) + part + b"\r\n"
    return out + b"0\r\n\r\n"


async def respond(path: str, req: bytes, w: asyncio.StreamWriter) -> str:
    name, _, ext = path.lstrip("/").rpartition(".")
    ctype = MIME.get(ext, "application/octet-stream")
    body = IMG.get(ext, BMP)
    tag = [b"ETag: " + etag(path)]
    if b"if-none-match: " + etag(path).lower() in req.lower():
        w.write(
            b"HTTP/1.1 304 Not Modified\r\nETag: "
            + etag(path)
            + b"\r\nConnection: close\r\n\r\n"
        )
        return "304 not modified"
    if name == "ok":
        w.write(headers(ctype, [b"Content-Length: %d" % len(body), *tag]) + body)
        return f"full {len(body)} bytes"
    if name == "truncated":  # cut one byte into the sixth pixel, Content-Length matches
        cut = BMP[: BMP_HDR + 5 * 3 + 1]
        w.write(headers(ctype, [b"Content-Length: %d" % len(cut)]) + cut)
        return f"truncated {len(cut)} bytes"
    if name == "large_header":
        w.write(
            headers(ctype, [b"Content-Length: %d" % len(LARGE_HEADER)]) + LARGE_HEADER
        )
        return f"large header {len(LARGE_HEADER)} bytes"
    if name == "empty":
        w.write(headers(ctype, [b"Content-Length: 0"]))
        return "empty body"
    if (
        name == "stall"
    ):  # claims the full length, sends part, then holds the socket open
        part = body[: BMP_HDR + 15]
        w.write(headers(ctype, [b"Content-Length: %d" % len(body)]) + part)
        await w.drain()
        await asyncio.sleep(40)
        return f"stalled after {len(part)} bytes, closed after 40 s"
    if name == "closed_early":  # claims the full length, sends part, closes at once
        part = body[: BMP_HDR + 15]
        w.write(headers(ctype, [b"Content-Length: %d" % len(body)]) + part)
        return f"closed after {len(part)} of {len(body)} bytes"
    if name == "slow":  # pauses under the timeout, then completes
        w.write(headers(ctype, [b"Content-Length: %d" % len(body)]) + body[:BMP_HDR])
        await w.drain()
        await asyncio.sleep(2.5)
        w.write(body[BMP_HDR:])
        return f"full {len(body)} bytes with a 2.5 s pause"
    if name == "chunked":
        w.write(
            headers(ctype, [b"Transfer-Encoding: chunked", *tag]) + chunked(body, 64)
        )
        return f"chunked full {len(body)} bytes"
    if (
        name == "chunked_cut"
    ):  # chunked, ends cleanly at a pixel boundary (or half a png)
        cut = body[: BMP_HDR + 5 * 3] if ext == "bmp" else body[: len(body) // 2]
        w.write(
            headers(ctype, [b"Transfer-Encoding: chunked", *tag]) + chunked(cut, 64)
        )
        return f"chunked cut {len(cut)} of {len(body)} bytes"
    if name == "noclen":  # body delimited by connection close
        w.write(headers(ctype, []) + body)
        return f"no content-length, full {len(body)} bytes"
    if name == "huge":  # claims 20 MB so the JPEG buffer resize fails
        w.write(headers(ctype, [b"Content-Length: 20000000"]) + body)
        return f"claims 20000000, sent {len(body)} bytes"
    prefix, _, rest = name.partition("_")
    if prefix in ("half", "noclen", "chunked", "stall") and rest:
        # 404 when the base file is missing, so a typo never passes as the misbehaviour under test
        file = www_file(rest + "." + ext)
    else:
        prefix, file = "", www_file(path.lstrip("/"))
    if file is None:
        w.write(NOT_FOUND)
        return "404"
    data = file.read_bytes()
    if prefix == "half":  # the first half of a file, Content-Length matching
        cut = data[: len(data) // 2]
        w.write(headers(ctype, [b"Content-Length: %d" % len(cut)]) + cut)
        return f"half {len(cut)} of {len(data)} bytes"
    if prefix == "noclen":  # a file without Content-Length, delimited by close
        w.write(headers(ctype, []) + data)
        return f"no content-length, full {len(data)} bytes"
    if prefix == "chunked":  # a file in 1 KB chunks
        w.write(headers(ctype, [b"Transfer-Encoding: chunked"]) + chunked(data, 1024))
        return f"chunked full {len(data)} bytes"
    if prefix == "stall":  # claims the full length, sends a third, holds the socket
        part = data[: len(data) // 3]
        w.write(headers(ctype, [b"Content-Length: %d" % len(data)]) + part)
        await w.drain()
        await asyncio.sleep(40)
        return f"stalled after {len(part)} of {len(data)} bytes"
    w.write(headers(ctype, [b"Content-Length: %d" % len(data), *tag]) + data)
    return f"file {file.name} {len(data)} bytes"


async def handle(r: asyncio.StreamReader, w: asyncio.StreamWriter) -> None:
    peer = w.get_extra_info("peername")
    try:
        async with asyncio.timeout(5):
            line = await r.readuntil(b"\r\n")
            req = await r.readuntil(b"\r\n\r\n")
        path = line.split(b" ")[1].decode()
        log(
            f"GET {path} from {peer}"
            + (" (If-None-Match)" if b"if-none-match" in req.lower() else "")
        )
        what = await respond(path, req, w)
        await w.drain()
        log(f"  -> {path}: {what}")
    except Exception as exc:  # noqa: BLE001
        log(f"  !! {peer}: {exc!r}")
    finally:
        w.close()


async def main() -> None:
    server = await asyncio.start_server(handle, "0.0.0.0", PORT)
    log(f"listening on {PORT}")
    async with server:
        await server.serve_forever()


asyncio.run(main())
