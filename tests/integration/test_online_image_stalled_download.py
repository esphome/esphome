"""Downloads that can never complete must report an error instead of waiting forever."""

from __future__ import annotations

import asyncio
import struct

import pytest

from .log_utils import LineWaiter
from .online_image_utils import BMP_IMAGE
from .types import APIClientConnectedFactory, RunCompiledFunction

# 24-bit BMP cut off one byte into the sixth pixel: the decoder keeps that byte,
# waiting for the rest of the pixel, but the response has already ended.
TRUNCATED_BMP = BMP_IMAGE[: 54 + 5 * 3 + 1]


def _make_palette_bmp() -> bytes:
    """Build an 8x8, 8-bit BMP whose 256-color table puts the pixels at offset 1078."""
    width = height = 8
    data_offset = 14 + 40 + 256 * 4
    file_size = data_offset + width * height
    file_header = b"BM" + struct.pack("<IHHI", file_size, 0, 0, data_offset)
    dib_header = struct.pack(
        "<IiiHHIIiiII", 40, width, height, 1, 8, 0, width * height, 0, 0, 256, 0
    )
    return file_header + dib_header + bytes(256 * 4) + bytes(width * height)


# The pixel data starts beyond a 256 byte download buffer, so the decoder can
# never get past the header with the data that fits in the buffer.
LARGE_HEADER_BMP = _make_palette_bmp()

IMAGES = {
    "/truncated.bmp": TRUNCATED_BMP,
    "/large_header.bmp": LARGE_HEADER_BMP,
    "/empty.bmp": b"",
}


async def handle_http(
    reader: asyncio.StreamReader, writer: asyncio.StreamWriter
) -> None:
    try:
        async with asyncio.timeout(1.0):
            request_line = await reader.readuntil(b"\r\n")
            await reader.readuntil(b"\r\n\r\n")
        path = request_line.split(b" ")[1].decode()
        body = IMAGES[path]
        writer.write(
            b"\r\n".join(
                [
                    b"HTTP/1.1 200 OK",
                    b"Content-Length: %d" % len(body),
                    b"Content-Type: image/bmp",
                    b"Connection: close",
                    b"",
                    b"",
                ]
            )
            + body
        )
        await writer.drain()
    finally:
        writer.close()


@pytest.mark.asyncio
async def test_online_image_stalled_download(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Esphome should report an error when a download can never complete."""
    waiter = LineWaiter()

    server = await asyncio.start_server(handle_http, "127.0.0.1", 0)
    http_server_port = server.sockets[0].getsockname()[1]
    config = yaml_config.replace("HTTP_PORT", str(http_server_port))

    async with (
        server,
        run_compiled(config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()
        actions = {s.name: s for s in services}

        await client.execute_service(actions["fetch_truncated"], {})
        async with asyncio.timeout(2.0):
            await waiter.wait_for_each(
                "Download ended with 1 bytes the decoder could not use",
                "download error: truncated",
            )

        await client.execute_service(actions["fetch_large_header"], {})
        async with asyncio.timeout(2.0):
            await waiter.wait_for_each(
                "Image does not fit in the 256 byte download buffer",
                "download error: large_header",
            )

        await client.execute_service(actions["fetch_empty"], {})
        async with asyncio.timeout(2.0):
            await waiter.wait_for_each(
                "Server sent an empty response", "download error: empty"
            )

    finished_lines = [line for line in waiter.lines if "download finished" in line]
    assert not finished_lines, f"Unexpected successful download: {finished_lines}"
