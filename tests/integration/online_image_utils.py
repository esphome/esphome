"""Shared fixture server and log helpers for the online_image integration tests."""

from __future__ import annotations

import asyncio
from collections.abc import Callable
import re

# black 8x8 RGB BMP, generated with
#   from PIL import Image
#   from io import BytesIO
#   b = BytesIO()
#   img = Image.new("RGB", (8, 8))
#   img.save(b, format="BMP")
#   b.getvalue()
BMP_IMAGE = b"BM\xf6\x00\x00\x00\x00\x00\x00\x006\x00\x00\x00(\x00\x00\x00\x08\x00\x00\x00\x08\x00\x00\x00\x01\x00\x18\x00\x00\x00\x00\x00\xc0\x00\x00\x00\xc4\x0e\x00\x00\xc4\x0e\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
LEN_BMP_IMAGE = len(BMP_IMAGE)


async def wait_for_download(
    downloaded_bytes_future: asyncio.Future,
    server_error_future: asyncio.Future,
) -> int:
    """Await the downloaded byte count, raising a server handler error first."""
    await asyncio.wait(
        {downloaded_bytes_future, server_error_future},
        return_when=asyncio.FIRST_COMPLETED,
    )
    if server_error_future.done() and (exc := server_error_future.exception()):
        raise exc
    # Retrieve a late teardown error so asyncio does not log it at GC
    server_error_future.add_done_callback(lambda f: f.exception())
    return downloaded_bytes_future.result()


def make_download_watcher(
    downloaded_bytes_future: asyncio.Future,
    download_finished_future: asyncio.Future,
) -> Callable[[str], None]:
    """Build a line callback resolving the futures from the device log."""

    def check_output(line: str) -> None:
        if (
            match := re.search(r"Image fully downloaded, (\d+) bytes", line)
        ) and not downloaded_bytes_future.done():
            downloaded_bytes_future.set_result(int(match.group(1)))
        if "download finished" in line and not download_finished_future.done():
            download_finished_future.set_result(True)

    return check_output


def handle_http(
    http_request_future,
    content_type: str = "text/plain",
    *,
    request_path: str = "/foo.bmp",
    request_line_consumed: bool = False,
    server_error_future: asyncio.Future | None = None,
):
    async def handler(reader, writer):
        try:
            # Only read the request line if it hasn't been consumed by a caller
            if not request_line_consumed:
                async with asyncio.timeout(1.0):
                    data = await reader.readuntil(b"\r\n")

                expected_request = f"GET {request_path} HTTP/1.1\r\n".encode()
                assert data[: len(expected_request)] == expected_request

            async with asyncio.timeout(1.0):
                await reader.readuntil(b"\r\n\r\n")

            if not http_request_future.done():
                http_request_future.set_result(True)

            http_response = [
                b"HTTP/1.1 200 OK",
                b"Content-Length: %d" % LEN_BMP_IMAGE,
                f"Content-Type: {content_type}".encode(),
                b"Connection: close",
                b"",
                b"",
            ]
            writer.write(b"\r\n".join(http_response))
            await writer.drain()

            writer.write(BMP_IMAGE)

            await writer.drain()
        except Exception as exc:
            if server_error_future is not None and not server_error_future.done():
                server_error_future.set_exception(exc)
            if not http_request_future.done():
                http_request_future.set_exception(exc)
            raise
        finally:
            writer.close()

    return handler


def handle_http_redirect(
    http_request_future, final_request_future, server_error_future, port_holder
):
    async def handler(reader, writer):
        try:
            async with asyncio.timeout(1.0):
                request = await reader.readuntil(b"\r\n")

            if (
                request[: len(b"GET /foo.bmp HTTP/1.1\r\n")]
                == b"GET /foo.bmp HTTP/1.1\r\n"
            ):
                if not http_request_future.done():
                    http_request_future.set_result(True)
                async with asyncio.timeout(1.0):
                    await reader.readuntil(b"\r\n\r\n")

                http_response = [
                    b"HTTP/1.1 302 Found",
                    f"Location: http://127.0.0.1:{port_holder['port']}/final.bmp".encode(),
                    b"Content-Type: text/html",
                    b"Content-Length: 0",
                    b"Connection: close",
                    b"",
                    b"",
                ]
                writer.write(b"\r\n".join(http_response))
                await writer.drain()
                return

            assert (
                request[: len(b"GET /final.bmp HTTP/1.1\r\n")]
                == b"GET /final.bmp HTTP/1.1\r\n"
            )
            if not final_request_future.done():
                final_request_future.set_result(True)
            await handle_http(
                final_request_future,
                "image/bmp",
                request_path="/final.bmp",
                request_line_consumed=True,
                server_error_future=server_error_future,
            )(reader, writer)
        except Exception as exc:
            # Route handler failures to the dedicated error future so they're not silently lost
            if not server_error_future.done():
                server_error_future.set_exception(exc)
            if not http_request_future.done():
                http_request_future.set_exception(exc)
            if not final_request_future.done():
                final_request_future.set_exception(exc)
            raise
        finally:
            writer.close()

    return handler
