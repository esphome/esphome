from __future__ import annotations

import asyncio

import pytest

from .online_image_utils import LEN_BMP_IMAGE, handle_http, make_download_watcher
from .types import APIClientConnectedFactory, RunCompiledFunction


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

    check_output = make_download_watcher(
        downloaded_bytes_future, download_finished_future
    )

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
