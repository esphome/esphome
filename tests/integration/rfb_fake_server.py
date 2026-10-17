"""A deterministic fake RFB (VNC) server for the vnc integration tests.

It behaves like the TightVNC 1.3.10 Xvnc that the Home Assistant VNC add-on runs: the same
handshake for RFB 3.3, 3.7 and 3.8, the same security lists and failure messages, and a
connection that closes on any client message type it does not know.

The framebuffer is an RGB888 image the test draws into. Updates are translated into the pixel
format the client asked for and encoded as Raw, Hextile or a RichCursor shape. Every client
message is recorded, and the test decides when to answer: ``wait_for("fbur")`` then
``send_update(...)``.
"""

from __future__ import annotations

import asyncio
from collections import Counter
from collections.abc import Callable, Iterable, Sequence
import contextlib
from dataclasses import asdict, dataclass
import logging
import random
import re
import struct
from typing import Any, Literal, Self

from cryptography.hazmat.primitives.ciphers import Cipher, modes

try:
    from cryptography.hazmat.decrepit.ciphers.algorithms import TripleDES
except ImportError:  # cryptography < 43
    from cryptography.hazmat.primitives.ciphers.algorithms import TripleDES

_LOGGER = logging.getLogger(__name__)

SECURITY_NONE = 1
SECURITY_VNC_AUTH = 2
SECURITY_TIGHT = 16

ENCODING_RAW = 0
ENCODING_HEXTILE = 5
ENCODING_RICH_CURSOR = -239

HEXTILE_RAW = 0x01
HEXTILE_BACKGROUND = 0x02
HEXTILE_FOREGROUND = 0x04
HEXTILE_ANY_SUBRECTS = 0x08
HEXTILE_SUBRECTS_COLOURED = 0x10

#: ``chunk`` value that writes in random 1-7 byte pieces.
HOSTILE_CHUNK = -1

#: TightVNC closes the connection on a longer ClientCutText.
MAX_CLIENT_CUT_TEXT = 1 << 20

AUTH_FAILED_REASON = "Authentication failed"

RGB = tuple[int, int, int]
Encoding = Literal["raw", "hextile", "hextile-raw-tiles", "rich-cursor"]
Rect = tuple[int, int, int, int, Encoding]
Message = dict[str, Any]

_PIXEL_FORMAT = struct.Struct(">BBBBHHHBBB3x")
_VERSION_RE = re.compile(rb"RFB (\d{3})\.(\d{3})\n")
_TILE = 16


@dataclass(frozen=True)
class PixelFormat:
    """An RFB PIXEL_FORMAT. Field names match the keys of a set_pixel_format message."""

    bpp: int
    depth: int
    big_endian: bool
    true_colour: bool
    red_max: int
    green_max: int
    blue_max: int
    red_shift: int
    green_shift: int
    blue_shift: int

    @classmethod
    def unpack(cls, data: bytes) -> PixelFormat:
        """Decode the 16 byte wire form."""
        bpp, depth, big_endian, true_colour, *rest = _PIXEL_FORMAT.unpack(data)
        return cls(bpp, depth, bool(big_endian), bool(true_colour), *rest)

    def pack(self) -> bytes:
        """Encode the 16 byte wire form."""
        return _PIXEL_FORMAT.pack(*asdict(self).values())

    @property
    def bytes_per_pixel(self) -> int:
        """Bytes per pixel on the wire."""
        return self.bpp // 8

    def value_of(self, rgb: RGB) -> int:
        """The pixel value a real server sends for an RGB888 colour."""
        r, g, b = rgb
        value = (
            (((r * self.red_max + 127) // 255) << self.red_shift)
            | (((g * self.green_max + 127) // 255) << self.green_shift)
            | (((b * self.blue_max + 127) // 255) << self.blue_shift)
        )
        return value & ((1 << self.bpp) - 1)

    def to_bytes(self, value: int) -> bytes:
        """A pixel value in wire byte order."""
        return value.to_bytes(
            self.bytes_per_pixel, "big" if self.big_endian else "little"
        )


#: TightVNC's native format at ``-depth 16`` on a little-endian host.
RGB565_LE = PixelFormat(16, 16, False, True, 31, 63, 31, 11, 5, 0)
RGB332 = PixelFormat(8, 8, False, True, 7, 7, 3, 5, 2, 0)


def des_ecb(key: bytes, data: bytes) -> bytes:
    """Single DES in ECB mode, as TripleDES with the 8 byte key repeated."""
    encryptor = Cipher(TripleDES(key * 3), modes.ECB()).encryptor()
    return encryptor.update(data) + encryptor.finalize()


def vnc_auth_response(password: str, challenge: bytes) -> bytes:
    """The answer to a VNC authentication challenge.

    The key is the password cut or zero padded to 8 bytes, with the bits of each byte reversed.
    """
    key = password.encode()[:8].ljust(8, b"\0")
    return des_ecb(bytes(int(f"{b:08b}"[::-1], 2) for b in key), challenge)


def _string(text: str) -> bytes:
    data = text.encode()
    return struct.pack(">I", len(data)) + data


class _ProtocolError(Exception):
    """The client broke the protocol; TightVNC would close the connection."""


@dataclass(eq=False)
class _Session:
    reader: asyncio.StreamReader
    writer: asyncio.StreamWriter
    pixel_format: PixelFormat
    task: asyncio.Task[None] | None
    minor: int = 8


class FakeRfbServer:
    """An RFB server with a scripted framebuffer, serving one client at a time.

    A new connection closes the previous one, as the add-on's ``-nevershared`` does.
    """

    def __init__(
        self,
        width: int,
        height: int,
        *,
        password: str | None = None,
        version: bytes = b"RFB 003.008\n",
        security_types: list[int] | None = None,
        name: str = "fake rfb",
        challenge: bytes = bytes(range(16)),
        native_format: PixelFormat = RGB565_LE,
        refuse_with_reason: str | None = None,
        handshake_chunk: int | None = None,
        seed: int = 0,
    ) -> None:
        """Set up the server; nothing listens until ``start()``.

        Args:
            width: Framebuffer width.
            height: Framebuffer height.
            password: Offer VNC authentication with this password instead of None.
            version: The 12 byte ProtocolVersion the server sends.
            security_types: The 3.7+ list. Defaults to TightVNC's ``[1, 16]`` or ``[2, 16]``.
            name: Desktop name in ServerInit.
            challenge: The 16 byte VNC authentication challenge.
            native_format: Pixel format in ServerInit, used until SetPixelFormat.
            refuse_with_reason: Refuse every connection before authentication with this
                reason, as TightVNC does after too many failures.
            handshake_chunk: ``chunk`` used for the server's handshake writes.
            seed: Seed for the hostile chunk sizes.
        """
        self.width = width
        self.height = height
        self.password = password
        self.version = version
        self.security_types = (
            list(security_types)
            if security_types is not None
            else [
                SECURITY_NONE if password is None else SECURITY_VNC_AUTH,
                SECURITY_TIGHT,
            ]
        )
        self.name = name
        self.challenge = challenge
        self.native_format = native_format
        self.refuse_with_reason = refuse_with_reason
        self.handshake_chunk = handshake_chunk
        self.image = bytearray(width * height * 3)
        #: Every parsed client message, from every connection, in arrival order.
        self.messages: list[Message] = []
        self.protocol_errors: list[str] = []
        self.client_version: bytes | None = None
        self.shared_flag: int | None = None
        self.encodings: list[int] = []
        self._rng = random.Random(seed)
        self._server: asyncio.Server | None = None
        self._session: _Session | None = None
        self._open: set[_Session] = set()
        self._pending: list[Message] = []
        self._changed = asyncio.Event()
        self._write_lock = asyncio.Lock()
        self._port = 0
        self._connections = 0
        self._sessions = 0
        self._auth_failures = 0

    async def __aenter__(self) -> Self:
        """Start on a free loopback port if not started yet."""
        if self._server is None:
            await self.start()
        return self

    async def __aexit__(self, *exc_info: object) -> None:
        """Stop the server."""
        await self.stop()

    @property
    def port(self) -> int:
        """The bound port, 0 before ``start()``."""
        return self._port

    @property
    def connections(self) -> int:
        """TCP connections accepted so far."""
        return self._connections

    @property
    def sessions(self) -> int:
        """Connections that got as far as ServerInit."""
        return self._sessions

    @property
    def auth_failures(self) -> int:
        """Wrong VNC authentication responses so far."""
        return self._auth_failures

    @property
    def client_connected(self) -> bool:
        """True while a client connection is open."""
        return self._session is not None

    @property
    def pixel_format(self) -> PixelFormat:
        """The current client's pixel format, or the native one with no client."""
        return self._session.pixel_format if self._session else self.native_format

    async def start(self, host: str = "127.0.0.1", port: int = 0) -> int:
        """Listen and return the bound port."""
        self._server = await asyncio.start_server(self._handle, host, port)
        self._port = self._server.sockets[0].getsockname()[1]
        return self._port

    async def stop(self) -> None:
        """Close the listener and every connection, and wait for their handlers."""
        if (server := self._server) is None:
            return
        self._server = None
        server.close()
        for session in list(self._open):
            session.writer.transport.abort()
        if tasks := [s.task for s in self._open if s.task is not None]:
            _, pending = await asyncio.wait(tasks, timeout=5.0)
            for task in pending:
                task.cancel()
            if pending:
                await asyncio.wait(pending)
        await server.wait_closed()

    def set_pixel(self, x: int, y: int, rgb: RGB) -> None:
        """Set one framebuffer pixel."""
        self.fill_rect(x, y, 1, 1, rgb)

    def get_pixel(self, x: int, y: int) -> RGB:
        """Read one framebuffer pixel."""
        self._check_rect(x, y, 1, 1)
        offset = (y * self.width + x) * 3
        r, g, b = self.image[offset : offset + 3]
        return r, g, b

    def fill_rect(self, x: int, y: int, w: int, h: int, rgb: RGB) -> None:
        """Fill a framebuffer rectangle with one colour."""
        self._check_rect(x, y, w, h)
        row = bytes(rgb) * w
        for line in range(y, y + h):
            offset = (line * self.width + x) * 3
            self.image[offset : offset + w * 3] = row

    async def wait_until(
        self,
        predicate: Callable[[], bool],
        timeout: float = 5.0,
        what: str = "condition",
    ) -> None:
        """Wait until ``predicate()`` is true; it is checked after every server event.

        Raises:
            AssertionError: If it is still false after ``timeout``.
        """
        if not await self._wait(lambda: True if predicate() else None, timeout):
            raise AssertionError(f"{what} not met within {timeout}s; {self._summary()}")

    async def wait_for(
        self,
        type_name: str,
        timeout: float = 5.0,
        predicate: Callable[[Message], bool] | None = None,
    ) -> Message:
        """Return and consume the oldest unconsumed message of this type.

        Messages of other types stay queued, so the order of calls does not matter. Only the
        current connection's messages are queued: a new connection drops the old ones.

        Raises:
            AssertionError: If no matching message arrives within ``timeout``.
        """

        def take() -> Message | None:
            for index, message in enumerate(self._pending):
                if message["type"] == type_name and (
                    predicate is None or predicate(message)
                ):
                    return self._pending.pop(index)
            return None

        if (message := await self._wait(take, timeout)) is None:
            raise AssertionError(
                f"no {type_name} message within {timeout}s; {self._summary()}"
            )
        return message

    def pending(self) -> list[Message]:
        """Messages not consumed by ``wait_for()`` yet."""
        return list(self._pending)

    def discard_pending(self) -> None:
        """Drop every unconsumed message."""
        self._pending.clear()

    def build_update(
        self,
        rects: Sequence[Rect],
        *,
        pixel_format: PixelFormat | None = None,
        always_send_bg: bool = False,
        raw_tile_flags: int = HEXTILE_RAW,
    ) -> bytes:
        """Encode one FramebufferUpdate from the image.

        Each rect is ``(x, y, w, h, encoding)``. For ``rich-cursor`` x and y are the hotspot
        and the shape is a fixed pattern. ``hextile-raw-tiles`` sends every tile raw.

        Args:
            rects: The rectangles, in order.
            pixel_format: Format to encode in; defaults to the current client's.
            always_send_bg: Send the Hextile background on every tile, even when unchanged.
            raw_tile_flags: Subencoding byte of raw Hextile tiles. Bits other than Raw are
                ignored by decoders, so 0x1F is valid.
        """
        fmt = pixel_format or self.pixel_format
        if not raw_tile_flags & HEXTILE_RAW:
            raise ValueError("raw_tile_flags must include the Raw bit")
        out = bytearray(struct.pack(">BxH", 0, len(rects)))
        for x, y, w, h, encoding in rects:
            if encoding == "rich-cursor":
                out += struct.pack(">HHHHi", x, y, w, h, ENCODING_RICH_CURSOR)
                out += _cursor_shape(fmt, w, h)
                continue
            self._check_rect(x, y, w, h)
            values = self._values(fmt, x, y, w, h)
            if encoding == "raw":
                out += struct.pack(">HHHHi", x, y, w, h, ENCODING_RAW)
                out += _pixels(fmt, (v for row in values for v in row))
            elif encoding in ("hextile", "hextile-raw-tiles"):
                out += struct.pack(">HHHHi", x, y, w, h, ENCODING_HEXTILE)
                out += _hextile(
                    fmt,
                    values,
                    force_raw=encoding == "hextile-raw-tiles",
                    always_send_bg=always_send_bg,
                    raw_tile_flags=raw_tile_flags,
                )
            else:
                raise ValueError(f"unknown encoding {encoding!r}")
        return bytes(out)

    async def send_update(
        self,
        rects: Sequence[Rect],
        chunk: int | None = None,
        *,
        always_send_bg: bool = False,
        raw_tile_flags: int = HEXTILE_RAW,
        delay: float = 0.0,
    ) -> None:
        """Send one FramebufferUpdate built by ``build_update()``.

        Args:
            rects: The rectangles, in order.
            chunk: Split the write into pieces of at most this many bytes; ``HOSTILE_CHUNK``
                uses seeded random 1-7 byte pieces.
            always_send_bg: See ``build_update()``.
            raw_tile_flags: See ``build_update()``.
            delay: Sleep between pieces.
        """
        session = self._require_session()
        data = self.build_update(
            rects,
            pixel_format=session.pixel_format,
            always_send_bg=always_send_bg,
            raw_tile_flags=raw_tile_flags,
        )
        await self._write(session, data, chunk, delay)

    async def send_cut_text(
        self, text: bytes, chunk: int | None = None, *, delay: float = 0.0
    ) -> None:
        """Send ServerCutText."""
        await self.send_raw(
            struct.pack(">B3xI", 3, len(text)) + text, chunk, delay=delay
        )

    async def send_bell(self) -> None:
        """Send Bell."""
        await self.send_raw(b"\x02")

    async def send_colour_map(
        self,
        first: int = 0,
        colours: Sequence[RGB] = (),
        chunk: int | None = None,
    ) -> None:
        """Send SetColourMapEntries; colours are 16 bit per channel."""
        data = struct.pack(">BxHH", 1, first, len(colours))
        data += b"".join(struct.pack(">HHH", *colour) for colour in colours)
        await self.send_raw(data, chunk)

    async def send_raw(
        self, data: bytes, chunk: int | None = None, *, delay: float = 0.0
    ) -> None:
        """Send arbitrary bytes to the current client."""
        await self._write(self._require_session(), data, chunk, delay)

    async def close_client(self, *, abort: bool = False) -> None:
        """Close the current client connection and wait for its handler to finish."""
        if (session := self._session) is None:
            return
        if abort:
            session.writer.transport.abort()
        else:
            session.writer.close()
        if session.task is not None:
            await asyncio.wait([session.task], timeout=5.0)

    def _check_rect(self, x: int, y: int, w: int, h: int) -> None:
        if min(x, y, w, h) < 0 or x + w > self.width or y + h > self.height:
            raise ValueError(
                f"rect {w}x{h}+{x}+{y} is outside the {self.width}x{self.height} framebuffer"
            )

    def _values(
        self, fmt: PixelFormat, x: int, y: int, w: int, h: int
    ) -> list[list[int]]:
        cache: dict[bytes, int] = {}
        rows = []
        for line in range(y, y + h):
            offset = (line * self.width + x) * 3
            data = bytes(self.image[offset : offset + w * 3])
            row = []
            for i in range(0, len(data), 3):
                rgb = data[i : i + 3]
                if (value := cache.get(rgb)) is None:
                    value = cache[rgb] = fmt.value_of((rgb[0], rgb[1], rgb[2]))
                row.append(value)
            rows.append(row)
        return rows

    def _require_session(self) -> _Session:
        if self._session is None:
            raise RuntimeError("no RFB client is connected")
        return self._session

    def _summary(self) -> str:
        return (
            f"connections={self._connections}, sessions={self._sessions}, "
            f"auth_failures={self._auth_failures}, "
            f"pending={[m['type'] for m in self._pending]}, "
            f"protocol_errors={self.protocol_errors}"
        )

    def _bump(self) -> None:
        self._changed.set()
        self._changed = asyncio.Event()

    async def _wait[T](self, check: Callable[[], T | None], timeout: float) -> T | None:
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout
        while (result := check()) is None:
            if (remaining := deadline - loop.time()) <= 0:
                return None
            with contextlib.suppress(TimeoutError):
                await asyncio.wait_for(self._changed.wait(), remaining)
        return result

    async def _write(
        self, session: _Session, data: bytes, chunk: int | None, delay: float = 0.0
    ) -> None:
        if chunk is not None and chunk != HOSTILE_CHUNK and chunk < 1:
            raise ValueError(f"bad chunk size {chunk}")
        writer = session.writer
        async with self._write_lock:
            if chunk is None:
                writer.write(data)
                await writer.drain()
                return
            pos = 0
            while pos < len(data):
                size = self._rng.randint(1, 7) if chunk == HOSTILE_CHUNK else chunk
                writer.write(data[pos : pos + size])
                await writer.drain()
                pos += size
                await asyncio.sleep(delay)

    async def _reply(self, session: _Session, data: bytes) -> None:
        await self._write(session, data, self.handshake_chunk)

    def _record(self, session: _Session, message: Message) -> None:
        self.messages.append(message)
        if session is self._session:
            self._pending.append(message)
        self._bump()

    async def _handle(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        session = _Session(reader, writer, self.native_format, asyncio.current_task())
        self._connections += 1
        if (previous := self._session) is not None:
            previous.writer.close()
        self._session = session
        self._open.add(session)
        self._pending.clear()
        self._bump()
        try:
            await self._serve(session)
        except (asyncio.IncompleteReadError, ConnectionError):
            pass
        except _ProtocolError as err:
            self.protocol_errors.append(str(err))
        except Exception as err:  # noqa: BLE001 - reported through protocol_errors
            _LOGGER.exception("Fake RFB server failed")
            self.protocol_errors.append(f"internal error: {err!r}")
        finally:
            if self._session is session:
                self._session = None
            writer.close()
            with contextlib.suppress(OSError):
                await writer.wait_closed()
            self._open.discard(session)
            self._bump()

    async def _serve(self, session: _Session) -> None:
        await self._reply(session, self.version)
        session.minor = self._negotiate_version(await session.reader.readexactly(12))
        if not await self._negotiate_security(session):
            return
        self.shared_flag = (await session.reader.readexactly(1))[0]
        server_init = (
            struct.pack(">HH", self.width, self.height)
            + self.native_format.pack()
            + _string(self.name)
        )
        await self._reply(session, server_init)
        self._sessions += 1
        self._bump()
        while True:
            await self._read_message(session)

    def _negotiate_version(self, data: bytes) -> int:
        self.client_version = data
        if (match := _VERSION_RE.fullmatch(data)) is None:
            raise _ProtocolError(f"bad client version {data!r}")
        if int(match[1]) != 3:
            raise _ProtocolError(f"unsupported client version {data!r}")
        minor = int(match[2])
        if minor > 8:
            return 8
        return minor if minor in (7, 8) else 3

    async def _negotiate_security(self, session: _Session) -> bool:
        reason = self.refuse_with_reason
        if reason is None and not self.security_types:
            reason = ""
        if session.minor == 3:
            if reason is not None:
                await self._reply(session, struct.pack(">I", 0) + _string(reason))
                return False
            chosen = next(
                (
                    t
                    for t in self.security_types
                    if t in (SECURITY_NONE, SECURITY_VNC_AUTH)
                ),
                self.security_types[0],
            )
            await self._reply(session, struct.pack(">I", chosen))
        else:
            if reason is not None:
                await self._reply(session, b"\0" + _string(reason))
                return False
            offer = bytes([len(self.security_types), *self.security_types])
            await self._reply(session, offer)
            chosen = (await session.reader.readexactly(1))[0]
            if chosen not in self.security_types:
                raise _ProtocolError(
                    f"client chose security type {chosen}, offered {self.security_types}"
                )
        if chosen == SECURITY_NONE:
            if session.minor >= 8:
                await self._reply(session, struct.pack(">I", 0))
            return True
        if chosen == SECURITY_VNC_AUTH:
            return await self._vnc_auth(session)
        raise _ProtocolError(f"security type {chosen} is not emulated")

    async def _vnc_auth(self, session: _Session) -> bool:
        await self._reply(session, self.challenge)
        response = await session.reader.readexactly(16)
        if self.password is not None and response == vnc_auth_response(
            self.password, self.challenge
        ):
            await self._reply(session, struct.pack(">I", 0))
            return True
        self._auth_failures += 1
        self._bump()
        failure = struct.pack(">I", 1)
        if session.minor >= 8:
            failure += _string(AUTH_FAILED_REASON)
        await self._reply(session, failure)
        return False

    async def _read_message(self, session: _Session) -> None:
        read = session.reader.readexactly
        match kind := (await read(1))[0]:
            case 0:
                fmt = PixelFormat.unpack((await read(19))[3:])
                self._record(session, {"type": "set_pixel_format", **asdict(fmt)})
                if fmt.bpp not in (8, 16, 32):
                    raise _ProtocolError(f"unsupported bits per pixel {fmt.bpp}")
                if not fmt.true_colour:
                    raise _ProtocolError("colour map formats are not emulated")
                session.pixel_format = fmt
            case 2:
                (count,) = struct.unpack(">xH", await read(3))
                self.encodings = list(
                    struct.unpack(f">{count}i", await read(4 * count))
                )
                self._record(
                    session, {"type": "set_encodings", "encodings": self.encodings}
                )
            case 3:
                incremental, x, y, w, h = struct.unpack(">BHHHH", await read(9))
                self._record(
                    session,
                    {
                        "type": "fbur",
                        "incremental": bool(incremental),
                        "x": x,
                        "y": y,
                        "w": w,
                        "h": h,
                    },
                )
            case 4:
                down, keysym = struct.unpack(">B2xI", await read(7))
                self._record(
                    session, {"type": "key", "down": bool(down), "keysym": keysym}
                )
            case 5:
                mask, x, y = struct.unpack(">BHH", await read(5))
                self._record(session, {"type": "pointer", "mask": mask, "x": x, "y": y})
            case 6:
                (length,) = struct.unpack(">3xI", await read(7))
                if length > MAX_CLIENT_CUT_TEXT:
                    raise _ProtocolError(f"client cut text of {length} bytes")
                self._record(session, {"type": "cut_text", "text": await read(length)})
            case _:
                raise _ProtocolError(f"unknown client message type {kind}")


def _pixels(fmt: PixelFormat, values: Iterable[int]) -> bytes:
    cache: dict[int, bytes] = {}
    out = bytearray()
    for value in values:
        if (data := cache.get(value)) is None:
            data = cache[value] = fmt.to_bytes(value)
        out += data
    return bytes(out)


def _cursor_shape(fmt: PixelFormat, w: int, h: int) -> bytes:
    light = fmt.to_bytes(fmt.value_of((255, 255, 255)))
    dark = fmt.to_bytes(fmt.value_of((0, 0, 0)))
    pixels = b"".join(
        light if (x + y) % 2 else dark for y in range(h) for x in range(w)
    )
    return pixels + b"\x5a" * (((w + 7) // 8) * h)


def _subrects(tile: list[list[int]], bg: int) -> list[tuple[int, int, int, int, int]]:
    """Cover every non-background pixel with same-coloured rects, greedily."""
    height, width = len(tile), len(tile[0])
    done = [[False] * width for _ in range(height)]
    rects = []
    for y in range(height):
        for x in range(width):
            colour = tile[y][x]
            if colour == bg or done[y][x]:
                continue
            right = x
            while (
                right + 1 < width
                and tile[y][right + 1] == colour
                and not done[y][right + 1]
            ):
                right += 1
            bottom = y
            while bottom + 1 < height and all(
                tile[bottom + 1][i] == colour and not done[bottom + 1][i]
                for i in range(x, right + 1)
            ):
                bottom += 1
            for line in range(y, bottom + 1):
                done[line][x : right + 1] = [True] * (right + 1 - x)
            rects.append((x, y, right + 1 - x, bottom + 1 - y, colour))
    return rects


def _hextile(
    fmt: PixelFormat,
    values: list[list[int]],
    *,
    force_raw: bool,
    always_send_bg: bool,
    raw_tile_flags: int,
) -> bytes:
    """Hextile-encode a region given as rows of pixel values.

    Background and foreground carry over between tiles, but never past a raw tile or a
    coloured-subrect tile, as libvncserver's encoder does.
    """
    out = bytearray()
    height = len(values)
    width = len(values[0]) if values else 0
    bg: int | None = None
    fg: int | None = None
    for ty in range(0, height, _TILE):
        for tx in range(0, width, _TILE):
            tile = [row[tx : tx + _TILE] for row in values[ty : ty + _TILE]]
            flat = [v for row in tile for v in row]
            raw_size = len(flat) * fmt.bytes_per_pixel
            if not force_raw:
                counts = Counter(flat)
                tile_bg = counts.most_common(1)[0][0]
                flags = 0
                body = bytearray()
                if tile_bg != bg or always_send_bg:
                    flags |= HEXTILE_BACKGROUND
                    body += fmt.to_bytes(tile_bg)
                if len(counts) == 1:
                    out.append(flags)
                    out += body
                    bg = tile_bg
                    continue
                subrects = _subrects(tile, tile_bg)
                coloured = len(counts) > 2
                flags |= HEXTILE_ANY_SUBRECTS
                if coloured:
                    flags |= HEXTILE_SUBRECTS_COLOURED
                    tile_fg = None
                else:
                    tile_fg = next(c for c in counts if c != tile_bg)
                    if tile_fg != fg:
                        flags |= HEXTILE_FOREGROUND
                        body += fmt.to_bytes(tile_fg)
                body.append(len(subrects))
                for x, y, w, h, colour in subrects:
                    if coloured:
                        body += fmt.to_bytes(colour)
                    body += bytes([(x << 4) | y, ((w - 1) << 4) | (h - 1)])
                if len(body) < raw_size:
                    out.append(flags)
                    out += body
                    bg, fg = tile_bg, tile_fg
                    continue
            out.append(raw_tile_flags)
            out += _pixels(fmt, flat)
            bg = fg = None
    return bytes(out)
