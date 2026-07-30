from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

# black 8x8 RGB BMP, generated with
#   from PIL import Image
#   from io import BytesIO
#   b = BytesIO()
#   img = Image.new("RGB", (8, 8))
#   img.save(b, format="BMP")
#   b.getvalue()
BMP_IMAGE = b"BM\xf6\x00\x00\x00\x00\x00\x00\x006\x00\x00\x00(\x00\x00\x00\x08\x00\x00\x00\x08\x00\x00\x00\x01\x00\x18\x00\x00\x00\x00\x00\xc0\x00\x00\x00\xc4\x0e\x00\x00\xc4\x0e\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
LEN_BMP_IMAGE = len(BMP_IMAGE)


def handle_http(http_request_future):
    async def handler(reader, writer):
        try:
            async with asyncio.timeout(1.0):
                data = await reader.readuntil(b"\r\n")

            # ensure our request matches the expectation
            expected_request = b"GET /foo.bmp HTTP/1.1\r\n"
            assert data[: len(expected_request)] == expected_request

            # consume rest of request
            async with asyncio.timeout(1.0):
                data = await reader.readuntil(b"\r\n\r\n")

            http_request_future.set_result(True)

            http_response = [
                b"HTTP/1.1 200 OK",
                b"Content-Length: %d" % LEN_BMP_IMAGE,
                b"Content-Type: text/plain",
                b"Connection: close",
                b"",
                b"",
            ]
            writer.write(b"\r\n".join(http_response))
            await writer.drain()

            writer.write(BMP_IMAGE)

            await writer.drain()
        except Exception as exc:
            if not http_request_future.done():
                http_request_future.set_exception(exc)
            raise
        finally:
            writer.close()

    return handler


@pytest.mark.asyncio
async def test_online_image_bmp(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Esphome shouldn't block the main loop when a http response is slow"""
    loop = asyncio.get_running_loop()

    # Track http request
    http_request_future = loop.create_future()
    download_finished_future = loop.create_future()
    downloaded_bytes_future = loop.create_future()

    def check_output(line: str) -> None:
        """Check log output for expected messages."""

        if match := re.search(r"Image fully downloaded, (\d+) bytes", line):
            downloaded_bytes_future.set_result(int(match.group(1)))

        if "download finished" in line:
            download_finished_future.set_result(True)

    server = await asyncio.start_server(
        handle_http(http_request_future), "127.0.0.1", 0
    )
    http_server_port = server.sockets[0].getsockname()[1]

    config = yaml_config.replace("HTTP_PORT", str(http_server_port))

    # Run with log monitoring
    async with (
        server,
        run_compiled(config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info

        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "online-image-bmp"

        # List services to find our test service
        _, services = await client.list_entities_services()

        # Find test service
        request_service = next((s for s in services if s.name == "fetch_image"), None)

        assert request_service is not None, "fetch_image service not found"

        await client.execute_service(request_service, {})

        async with asyncio.timeout(0.1):
            await http_request_future

        async with asyncio.timeout(0.5):
            numbytes = await downloaded_bytes_future
            assert numbytes == LEN_BMP_IMAGE
            await download_finished_future
