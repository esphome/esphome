"""Test that online_image AUTO format detection reads the Content-Type header."""

from __future__ import annotations

import asyncio

import pytest

from .log_utils import LineWaiter
from .online_image_utils import (
    DOWNLOADED_BYTES,
    LEN_BMP_IMAGE,
    handle_http,
    wait_for_download,
)
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_online_image_auto_detects_image_bmp_mime(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """AUTO format detection should honor the final response MIME type without explicit format."""
    loop = asyncio.get_running_loop()
    http_request_future = loop.create_future()
    server_error_future = loop.create_future()
    waiter = LineWaiter()
    downloaded_bytes_future = waiter.expect(DOWNLOADED_BYTES)
    download_finished_future = waiter.expect("download finished")

    server = await asyncio.start_server(
        handle_http(
            http_request_future,
            "image/bmp",
            server_error_future=server_error_future,
        ),
        "127.0.0.1",
        0,
    )
    http_server_port = server.sockets[0].getsockname()[1]

    config = yaml_config.replace("HTTP_PORT", str(http_server_port))

    async with (
        server,
        run_compiled(config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "online-image-bmp")

        _, services = await client.list_entities_services()
        request_service = require_service(services, "fetch_image")

        await client.execute_service(request_service, {})

        async with asyncio.timeout(0.1):
            await http_request_future

        async with asyncio.timeout(0.5):
            numbytes = await wait_for_download(
                downloaded_bytes_future, server_error_future
            )
            assert numbytes == LEN_BMP_IMAGE
            await download_finished_future
