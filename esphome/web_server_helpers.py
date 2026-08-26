"""Shared helpers for the web_server HTTP transports (OTA upload and logs)."""

from __future__ import annotations

from esphome.const import (
    CONF_AUTH,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_USERNAME,
    CONF_WEB_SERVER,
)
from esphome.core import CORE, EsphomeError
from esphome.helpers import format_ip_url, resolve_ip_address
from esphome.types import ConfigType


def resolve_web_server_urls(host: str, port: int, path: str) -> list[tuple[str, str]]:
    """Resolve ``host`` to ``(ip, url)`` pairs for the web_server ``path``.

    Wraps :func:`resolve_ip_address` (honoring ``CORE.address_cache``) and
    formats each resolved address into an ``http://host:port/path`` URL via
    :func:`format_ip_url`, handling both IPv4 and IPv6. Shared by the
    web_server OTA upload and log streaming paths.
    """
    addr_infos = resolve_ip_address(host, port, address_cache=CORE.address_cache)
    return [
        (sockaddr[0], format_ip_url(family, sockaddr, port, path))
        for family, _socktype, _, _, sockaddr in addr_infos
    ]


def get_web_server_connection(config: ConfigType) -> tuple[int, str | None, str | None]:
    """Return ``(port, username, password)`` for the web_server HTTP endpoint.

    Reads the port and optional HTTP Basic-auth credentials from the validated
    ``web_server:`` config, shared by the web_server OTA upload and log
    streaming paths. Raises :class:`EsphomeError` if ``web_server`` is absent.
    """
    web_conf = config.get(CONF_WEB_SERVER)
    if not web_conf:
        raise EsphomeError(f"The {CONF_WEB_SERVER} component is not configured.")
    auth = web_conf.get(CONF_AUTH) or {}
    return int(web_conf[CONF_PORT]), auth.get(CONF_USERNAME), auth.get(CONF_PASSWORD)
