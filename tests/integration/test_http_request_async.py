"""Integration test for TemplatableStringValue with string lambdas."""

from __future__ import annotations

import asyncio

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


def handle_http(http_request_future):
    async def handler(reader, writer):
        try:
            async with asyncio.timeout(1.0):
                data = await reader.readuntil(b"\r\n")

            # ensure our request matches the expectation
            expected_request = b"POST /foo HTTP/1.1\r\n"
            assert data[: len(expected_request)] == expected_request

            # consume rest of request
            async with asyncio.timeout(1.0):
                data = await reader.readuntil(b"\r\n\r\n")

            http_request_future.set_result(True)

            http_response = [
                b"HTTP/1.1 200 OK",
                b"Content-Length: 4",
                b"Content-Type: text/plain",
                b"Connection: close",
                b"",
                b"",
            ]
            writer.write(b"\r\n".join(http_response))
            await writer.drain()

            await asyncio.sleep(1.0)

            writer.write(b"done")
            await writer.drain()
        except Exception as exc:
            if not http_request_future.done():
                http_request_future.set_exception(exc)
            raise
        finally:
            writer.close()

    return handler


@pytest.mark.asyncio
async def test_http_request_async(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Esphome shouldn't block the main loop when a http response is slow"""
    loop = asyncio.get_running_loop()

    # Track http request
    http_request_future = loop.create_future()
    http_response_future = loop.create_future()

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if "got response done" in line:
            http_response_future.set_result(True)

    server = await asyncio.start_server(
        handle_http(http_request_future), "127.0.0.1", 0
    )
    http_server_port = server.sockets[0].getsockname()[1]

    # Run with log monitoring
    async with (
        server,
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "http-request-async"

        # List services to find our test service
        _, services = await client.list_entities_services()

        # Find test service
        request_service = next(
            (s for s in services if s.name == "send_http_request"), None
        )
        assert request_service is not None, "send_http_request service not found"

        client.execute_service(request_service, {"port": http_server_port})

        async with asyncio.timeout(0.1):
            await http_request_future

        # Verify device is still responding
        async with asyncio.timeout(0.1):
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "http-request-async"

        async with asyncio.timeout(1.5):
            await http_response_future
