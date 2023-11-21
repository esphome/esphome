from __future__ import annotations

import asyncio
import os
import socket

from esphome.storage_json import EsphomeStorageJSON, esphome_storage_path

from .core import DASHBOARD
from .web_server import make_app, start_web_server

ENV_DEV = "ESPHOME_DASHBOARD_DEV"

settings = DASHBOARD.settings


def start_dashboard(args) -> None:
    """Start the dashboard."""
    settings.parse_args(args)

    if settings.using_auth:
        path = esphome_storage_path()
        storage = EsphomeStorageJSON.load(path)
        if storage is None:
            storage = EsphomeStorageJSON.get_default()
            storage.save(path)
        settings.cookie_secret = storage.cookie_secret

    try:
        asyncio.run(async_start(args))
    except KeyboardInterrupt:
        pass


async def async_start(args) -> None:
    """Start the dashboard."""
    dashboard = DASHBOARD
    await dashboard.async_setup()
    sock: socket.socket | None = args.socket
    address: str | None = args.address
    port: int | None = args.port

    start_web_server(make_app(args.verbose), sock, address, port, settings.config_dir)

    if args.open_ui:
        import webbrowser

        webbrowser.open(f"http://{args.address}:{args.port}")

    try:
        await dashboard.async_run()
    finally:
        if sock:
            os.remove(sock)
