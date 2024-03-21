from __future__ import annotations

import asyncio
import json
import os
from unittest.mock import Mock

import pytest
import pytest_asyncio
from tornado.httpclient import AsyncHTTPClient, HTTPResponse
from tornado.httpserver import HTTPServer
from tornado.ioloop import IOLoop
from tornado.testing import bind_unused_port

from esphome.dashboard import web_server
from esphome.dashboard.core import DASHBOARD

from .common import get_fixture_path


class DashboardTestHelper:
    def __init__(self, io_loop: IOLoop, client: AsyncHTTPClient, port: int) -> None:
        self.io_loop = io_loop
        self.client = client
        self.port = port

    async def fetch(self, path: str, **kwargs) -> HTTPResponse:
        """Get a response for the given path."""
        if path.lower().startswith(("http://", "https://")):
            url = path
        else:
            url = f"http://127.0.0.1:{self.port}{path}"
        future = self.client.fetch(url, raise_error=True, **kwargs)
        result = await future
        return result


@pytest_asyncio.fixture()
async def dashboard() -> DashboardTestHelper:
    sock, port = bind_unused_port()
    args = Mock(
        ha_addon=True,
        configuration=get_fixture_path("conf"),
        port=port,
    )
    DASHBOARD.settings.parse_args(args)
    app = web_server.make_app()
    http_server = HTTPServer(app)
    http_server.add_sockets([sock])
    await DASHBOARD.async_setup()
    os.environ["DISABLE_HA_AUTHENTICATION"] = "1"
    assert DASHBOARD.settings.using_password is False
    assert DASHBOARD.settings.on_ha_addon is True
    assert DASHBOARD.settings.using_auth is False
    task = asyncio.create_task(DASHBOARD.async_run())
    client = AsyncHTTPClient()
    io_loop = IOLoop(make_current=False)
    yield DashboardTestHelper(io_loop, client, port)
    task.cancel()
    sock.close()
    client.close()
    io_loop.close()


@pytest.mark.asyncio
async def test_main_page(dashboard: DashboardTestHelper) -> None:
    response = await dashboard.fetch("/")
    assert response.code == 200


@pytest.mark.asyncio
async def test_devices_page(dashboard: DashboardTestHelper) -> None:
    response = await dashboard.fetch("/devices")
    assert response.code == 200
    assert response.headers["content-type"] == "application/json"
    json_data = json.loads(response.body.decode())
    configured_devices = json_data["configured"]
    first_device = configured_devices[0]
    assert first_device["name"] == "pico"
    assert first_device["configuration"] == "pico.yaml"
