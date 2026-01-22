from __future__ import annotations

import asyncio
import base64
import binascii
from collections.abc import Callable, Iterable
import contextlib
import datetime
import functools
from functools import partial
import gzip
import hashlib
import importlib
import json
import logging
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import threading
import time
from typing import TYPE_CHECKING, Any, TypeVar
from urllib.parse import urlparse

import tornado
import tornado.concurrent
import tornado.gen
import tornado.httpserver
import tornado.httputil
import tornado.ioloop
import tornado.iostream
from tornado.log import access_log
import tornado.netutil
import tornado.process
import tornado.queues
import tornado.web
import tornado.websocket
import voluptuous as vol
import yaml
from yaml.nodes import Node

from esphome import const, platformio_api, yaml_util
from esphome.helpers import get_bool_env, mkdir_p, sort_ip_addresses
from esphome.storage_json import (
    StorageJSON,
    archive_storage_path,
    ext_storage_path,
    trash_storage_path,
)
from esphome.util import get_serial_ports, shlex_quote
from esphome.yaml_util import FastestAvailableSafeLoader

from ..helpers import write_file
from .const import DASHBOARD_COMMAND, DashboardEvent
from .core import DASHBOARD, ESPHomeDashboard, Event
from .entries import UNKNOWN_STATE, DashboardEntry, entry_state_to_bool
from .models import build_device_list_response
from .util.subprocess import async_run_system_command
from .util.text import friendly_name_slugify

if TYPE_CHECKING:
    from requests import Response

_LOGGER = logging.getLogger(__name__)

ENV_DEV = "ESPHOME_DASHBOARD_DEV"

COOKIE_AUTHENTICATED_YES = b"yes"

AUTH_COOKIE_NAME = "authenticated"


settings = DASHBOARD.settings


def template_args() -> dict[str, Any]:
    version = const.__version__
    if "b" in version:
        docs_link = "https://beta.esphome.io/"
    elif "dev" in version:
        docs_link = "https://next.esphome.io/"
    else:
        docs_link = "https://www.esphome.io/"

    return {
        "version": version,
        "docs_link": docs_link,
        "get_static_file_url": get_static_file_url,
        "relative_url": settings.relative_url,
        "streamer_mode": settings.streamer_mode,
        "config_dir": settings.config_dir,
    }


T = TypeVar("T", bound=Callable[..., Any])


def authenticated(func: T) -> T:
    @functools.wraps(func)
    def decorator(self, *args: Any, **kwargs: Any):
        if not is_authenticated(self):
            self.redirect("./login")
            return None
        return func(self, *args, **kwargs)

    return decorator


def is_authenticated(handler: BaseHandler) -> bool:
    """Check if the request is authenticated."""
    if settings.on_ha_addon:
        # Handle ingress - disable auth on ingress port
        # X-HA-Ingress is automatically stripped on the non-ingress server in nginx
        header = handler.request.headers.get("X-HA-Ingress", "NO")
        if str(header) == "YES":
            return True

    if settings.using_auth:
        if auth_header := handler.request.headers.get("Authorization"):
            assert isinstance(auth_header, str)
            if auth_header.startswith("Basic "):
                auth_decoded = base64.b64decode(auth_header[6:]).decode()
                username, password = auth_decoded.split(":", 1)
                return settings.check_password(username, password)
        return handler.get_secure_cookie(AUTH_COOKIE_NAME) == COOKIE_AUTHENTICATED_YES

    return True


def bind_config(func):
    def decorator(self, *args, **kwargs):
        configuration = self.get_argument("configuration")
        kwargs = kwargs.copy()
        kwargs["configuration"] = configuration
        return func(self, *args, **kwargs)

    return decorator


# pylint: disable=abstract-method
class BaseHandler(tornado.web.RequestHandler):
    pass


def websocket_class(cls):
    # pylint: disable=protected-access
    if not hasattr(cls, "_message_handlers"):
        cls._message_handlers = {}

    for method in cls.__dict__.values():
        if hasattr(method, "_message_handler"):
            cls._message_handlers[method._message_handler] = method

    return cls


def websocket_method(name):
    def wrap(fn):
        # pylint: disable=protected-access
        fn._message_handler = name
        return fn

    return wrap


class CheckOriginMixin:
    """Mixin to handle WebSocket origin checks for reverse proxy setups."""

    def check_origin(self, origin: str) -> bool:
        if "ESPHOME_TRUSTED_DOMAINS" not in os.environ:
            return super().check_origin(origin)
        trusted_domains = [
            s.strip() for s in os.environ["ESPHOME_TRUSTED_DOMAINS"].split(",")
        ]
        url = urlparse(origin)
        if url.hostname in trusted_domains:
            return True
        _LOGGER.info("check_origin %s, domain is not trusted", origin)
        return False


@websocket_class
class EsphomeCommandWebSocket(CheckOriginMixin, tornado.websocket.WebSocketHandler):
    """Base class for ESPHome websocket commands."""

    def __init__(
        self,
        application: tornado.web.Application,
        request: tornado.httputil.HTTPServerRequest,
        **kwargs: Any,
    ) -> None:
        """Initialize the websocket."""
        super().__init__(application, request, **kwargs)
        self._proc = None
        self._queue = None
        self._is_closed = False
        # Windows doesn't support non-blocking pipes,
        # use Popen() with a reading thread instead
        self._use_popen = os.name == "nt"

    def open(self, *args: str, **kwargs: str) -> None:
        """Handle new WebSocket connection."""
        # Ensure messages from the subprocess are sent immediately
        # to avoid a 200-500ms delay when nodelay is not set.
        self.set_nodelay(True)

    @authenticated
    async def on_message(  # pylint: disable=invalid-overridden-method
        self, message: str
    ) -> None:
        # Since tornado 4.5, on_message is allowed to be a coroutine
        # Messages are always JSON, 500 when not
        json_message = json.loads(message)
        type_ = json_message["type"]
        # pylint: disable=no-member
        handlers = type(self)._message_handlers
        if type_ not in handlers:
            _LOGGER.warning("Requested unknown message type %s", type_)
            return

        await handlers[type_](self, json_message)

    @websocket_method("spawn")
    async def handle_spawn(self, json_message: dict[str, Any]) -> None:
        if self._proc is not None:
            # spawn can only be called once
            return
        command = await self.build_command(json_message)
        _LOGGER.info("Running command '%s'", " ".join(shlex_quote(x) for x in command))

        if self._use_popen:
            self._queue = tornado.queues.Queue()
            # pylint: disable=consider-using-with
            self._proc = subprocess.Popen(
                command,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                close_fds=False,
            )
            stdout_thread = threading.Thread(target=self._stdout_thread)
            stdout_thread.daemon = True
            stdout_thread.start()
        else:
            self._proc = tornado.process.Subprocess(
                command,
                stdout=tornado.process.Subprocess.STREAM,
                stderr=subprocess.STDOUT,
                stdin=tornado.process.Subprocess.STREAM,
                close_fds=False,
            )
            self._proc.set_exit_callback(self._proc_on_exit)

        tornado.ioloop.IOLoop.current().spawn_callback(self._redirect_stdout)

    @property
    def is_process_active(self) -> bool:
        return self._proc is not None and self._proc.returncode is None

    @websocket_method("stdin")
    async def handle_stdin(self, json_message: dict[str, Any]) -> None:
        if not self.is_process_active:
            return
        text: str = json_message["data"]
        data = text.encode("utf-8", "replace")
        _LOGGER.debug("< stdin: %s", data)
        self._proc.stdin.write(data)

    @tornado.gen.coroutine
    def _redirect_stdout(self) -> None:
        reg = b"[\n\r]"

        while True:
            try:
                if self._use_popen:
                    data: bytes = yield self._queue.get()
                    if data is None:
                        self._proc_on_exit(self._proc.poll())
                        break
                else:
                    data: bytes = yield self._proc.stdout.read_until_regex(reg)
            except tornado.iostream.StreamClosedError:
                break

            text = data.decode("utf-8", "replace")
            _LOGGER.debug("> stdout: %s", text)
            self.write_message({"event": "line", "data": text})

    def _stdout_thread(self) -> None:
        if not self._use_popen:
            return
        line = b""
        cr = False
        while True:
            data = self._proc.stdout.read(1)
            if data:
                if data == b"\r":
                    cr = True
                elif data == b"\n":
                    self._queue.put_nowait(line + b"\n")
                    line = b""
                    cr = False
                elif cr:
                    self._queue.put_nowait(line + b"\r")
                    line = data
                    cr = False
                else:
                    line += data
            if self._proc.poll() is not None:
                break
        self._proc.wait(1.0)
        self._queue.put_nowait(None)

    def _proc_on_exit(self, returncode: int) -> None:
        if not self._is_closed:
            # Check if the proc was not forcibly closed
            _LOGGER.info("Process exited with return code %s", returncode)
            self.write_message({"event": "exit", "code": returncode})

    def on_close(self) -> None:
        # Check if proc exists (if 'start' has been run)
        if self.is_process_active:
            _LOGGER.debug("Terminating process")
            if self._use_popen:
                self._proc.terminate()
            else:
                self._proc.proc.terminate()
        # Shutdown proc on WS close
        self._is_closed = True

    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        raise NotImplementedError


def build_cache_arguments(
    entry: DashboardEntry | None,
    dashboard: ESPHomeDashboard,
    now: float,
) -> list[str]:
    """Build cache arguments for passing to CLI.

    Args:
        entry: Dashboard entry for the configuration
        dashboard: Dashboard instance with cache access
        now: Current monotonic time for DNS cache expiry checks

    Returns:
        List of cache arguments to pass to CLI
    """
    cache_args: list[str] = []

    if not entry:
        return cache_args

    _LOGGER.debug(
        "Building cache for entry (address=%s, name=%s)",
        entry.address,
        entry.name,
    )

    def add_cache_entry(hostname: str, addresses: list[str], cache_type: str) -> None:
        """Add a cache entry to the command arguments."""
        if not addresses:
            return
        normalized = hostname.rstrip(".").lower()
        cache_args.extend(
            [
                f"--{cache_type}-address-cache",
                f"{normalized}={','.join(sort_ip_addresses(addresses))}",
            ]
        )

    # Check entry.address for cached addresses
    if use_address := entry.address:
        if use_address.endswith(".local"):
            # mDNS cache for .local addresses
            if (mdns := dashboard.mdns_status) and (
                cached := mdns.get_cached_addresses(use_address)
            ):
                _LOGGER.debug("mDNS cache hit for %s: %s", use_address, cached)
                add_cache_entry(use_address, cached, "mdns")
        # DNS cache for non-.local addresses
        elif cached := dashboard.dns_cache.get_cached_addresses(use_address, now):
            _LOGGER.debug("DNS cache hit for %s: %s", use_address, cached)
            add_cache_entry(use_address, cached, "dns")

    # Check entry.name if we haven't already cached via address
    # For mDNS devices, entry.name typically doesn't have .local suffix
    if entry.name and not use_address:
        mdns_name = (
            f"{entry.name}.local" if not entry.name.endswith(".local") else entry.name
        )
        if (mdns := dashboard.mdns_status) and (
            cached := mdns.get_cached_addresses(mdns_name)
        ):
            _LOGGER.debug("mDNS cache hit for %s: %s", mdns_name, cached)
            add_cache_entry(mdns_name, cached, "mdns")

    return cache_args


class EsphomePortCommandWebSocket(EsphomeCommandWebSocket):
    """Base class for commands that require a port."""

    async def build_device_command(
        self, args: list[str], json_message: dict[str, Any]
    ) -> list[str]:
        """Build the command to run."""
        dashboard = DASHBOARD
        entries = dashboard.entries
        configuration = json_message["configuration"]
        config_file = settings.rel_path(configuration)
        port = json_message["port"]

        # Build cache arguments to pass to CLI
        cache_args: list[str] = []

        if (
            port == "OTA"  # pylint: disable=too-many-boolean-expressions
            and (entry := entries.get(config_file))
            and entry.loaded_integrations
            and "api" in entry.loaded_integrations
        ):
            cache_args = build_cache_arguments(entry, dashboard, time.monotonic())

        # Cache arguments must come before the subcommand
        cmd = [*DASHBOARD_COMMAND, *cache_args, *args, config_file, "--device", port]
        _LOGGER.debug("Built command: %s", cmd)
        return cmd


class EsphomeLogsHandler(EsphomePortCommandWebSocket):
    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        """Build the command to run."""
        return await self.build_device_command(["logs"], json_message)


class EsphomeRenameHandler(EsphomeCommandWebSocket):
    old_name: str

    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        config_file = settings.rel_path(json_message["configuration"])
        self.old_name = json_message["configuration"]
        return [
            *DASHBOARD_COMMAND,
            "rename",
            config_file,
            json_message["newName"],
        ]

    def _proc_on_exit(self, returncode):
        super()._proc_on_exit(returncode)

        if returncode != 0:
            return

        # Remove the old ping result from the cache
        entries = DASHBOARD.entries
        if entry := entries.get(self.old_name):
            entries.async_set_state(entry, UNKNOWN_STATE)


class EsphomeUploadHandler(EsphomePortCommandWebSocket):
    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        """Build the command to run."""
        return await self.build_device_command(["upload"], json_message)


class EsphomeRunHandler(EsphomePortCommandWebSocket):
    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        """Build the command to run."""
        return await self.build_device_command(["run"], json_message)


class EsphomeCompileHandler(EsphomeCommandWebSocket):
    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        config_file = settings.rel_path(json_message["configuration"])
        command = [*DASHBOARD_COMMAND, "compile"]
        if json_message.get("only_generate", False):
            command.append("--only-generate")
        command.append(config_file)
        return command


class EsphomeValidateHandler(EsphomeCommandWebSocket):
    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        config_file = settings.rel_path(json_message["configuration"])
        command = [*DASHBOARD_COMMAND, "config", config_file]
        if not settings.streamer_mode:
            command.append("--show-secrets")
        return command


class EsphomeCleanMqttHandler(EsphomeCommandWebSocket):
    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        config_file = settings.rel_path(json_message["configuration"])
        return [*DASHBOARD_COMMAND, "clean-mqtt", config_file]


class EsphomeCleanAllHandler(EsphomeCommandWebSocket):
    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        clean_build_dir = json_message.get("clean_build_dir", True)
        if clean_build_dir:
            return [*DASHBOARD_COMMAND, "clean-all", settings.config_dir]
        return [*DASHBOARD_COMMAND, "clean-all"]


class EsphomeCleanHandler(EsphomeCommandWebSocket):
    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        config_file = settings.rel_path(json_message["configuration"])
        return [*DASHBOARD_COMMAND, "clean", config_file]


class EsphomeVscodeHandler(EsphomeCommandWebSocket):
    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        return [*DASHBOARD_COMMAND, "-q", "vscode", "dummy"]


class EsphomeAceEditorHandler(EsphomeCommandWebSocket):
    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        return [*DASHBOARD_COMMAND, "-q", "vscode", "--ace", settings.config_dir]


class EsphomeUpdateAllHandler(EsphomeCommandWebSocket):
    async def build_command(self, json_message: dict[str, Any]) -> list[str]:
        return [*DASHBOARD_COMMAND, "update-all", settings.config_dir]


# Dashboard polling constants
DASHBOARD_POLL_INTERVAL = 2  # seconds
DASHBOARD_ENTRIES_UPDATE_INTERVAL = 10  # seconds
DASHBOARD_ENTRIES_UPDATE_ITERATIONS = (
    DASHBOARD_ENTRIES_UPDATE_INTERVAL // DASHBOARD_POLL_INTERVAL
)


class DashboardSubscriber:
    """Manages dashboard event polling task lifecycle based on active subscribers."""

    def __init__(self) -> None:
        """Initialize the dashboard subscriber."""
        self._subscribers: set[DashboardEventsWebSocket] = set()
        self._event_loop_task: asyncio.Task | None = None
        self._refresh_event: asyncio.Event = asyncio.Event()

    def subscribe(self, subscriber: DashboardEventsWebSocket) -> Callable[[], None]:
        """Subscribe to dashboard updates and start event loop if needed."""
        self._subscribers.add(subscriber)
        if not self._event_loop_task or self._event_loop_task.done():
            self._event_loop_task = asyncio.create_task(self._event_loop())
            _LOGGER.info("Started dashboard event loop")
        return partial(self._unsubscribe, subscriber)

    def _unsubscribe(self, subscriber: DashboardEventsWebSocket) -> None:
        """Unsubscribe from dashboard updates and stop event loop if no subscribers."""
        self._subscribers.discard(subscriber)
        if (
            not self._subscribers
            and self._event_loop_task
            and not self._event_loop_task.done()
        ):
            self._event_loop_task.cancel()
            self._event_loop_task = None
            _LOGGER.info("Stopped dashboard event loop - no subscribers")

    def request_refresh(self) -> None:
        """Signal the polling loop to refresh immediately."""
        self._refresh_event.set()

    async def _event_loop(self) -> None:
        """Run the event polling loop while there are subscribers."""
        dashboard = DASHBOARD
        entries_update_counter = 0

        while self._subscribers:
            # Signal that we need ping updates (non-blocking)
            dashboard.ping_request.set()
            if settings.status_use_mqtt:
                dashboard.mqtt_ping_request.set()

            # Check if it's time to update entries or if refresh was requested
            entries_update_counter += 1
            if (
                entries_update_counter >= DASHBOARD_ENTRIES_UPDATE_ITERATIONS
                or self._refresh_event.is_set()
            ):
                entries_update_counter = 0
                await dashboard.entries.async_request_update_entries()
                # Clear the refresh event if it was set
                self._refresh_event.clear()

            # Wait for either timeout or refresh event
            try:
                async with asyncio.timeout(DASHBOARD_POLL_INTERVAL):
                    await self._refresh_event.wait()
                    # If we get here, refresh was requested - continue loop immediately
            except TimeoutError:
                # Normal timeout - continue with regular polling
                pass


# Global dashboard subscriber instance
DASHBOARD_SUBSCRIBER = DashboardSubscriber()


@websocket_class
class DashboardEventsWebSocket(CheckOriginMixin, tornado.websocket.WebSocketHandler):
    """WebSocket handler for real-time dashboard events."""

    _event_listeners: list[Callable[[], None]] | None = None
    _dashboard_unsubscribe: Callable[[], None] | None = None

    async def get(self, *args: str, **kwargs: str) -> None:
        """Handle WebSocket upgrade request."""
        if not is_authenticated(self):
            self.set_status(401)
            self.finish("Unauthorized")
            return
        await super().get(*args, **kwargs)

    async def open(self, *args: str, **kwargs: str) -> None:  # pylint: disable=invalid-overridden-method
        """Handle new WebSocket connection."""
        # Ensure messages are sent immediately to avoid
        # a 200-500ms delay when nodelay is not set.
        self.set_nodelay(True)

        # Update entries first
        await DASHBOARD.entries.async_request_update_entries()
        # Send initial state
        self._send_initial_state()
        # Subscribe to events
        self._subscribe_to_events()
        # Subscribe to dashboard updates
        self._dashboard_unsubscribe = DASHBOARD_SUBSCRIBER.subscribe(self)
        _LOGGER.debug("Dashboard status WebSocket opened")

    def _send_initial_state(self) -> None:
        """Send initial device list and ping status."""
        entries = DASHBOARD.entries.async_all()

        # Send initial state
        self._safe_send_message(
            {
                "event": DashboardEvent.INITIAL_STATE,
                "data": {
                    "devices": build_device_list_response(DASHBOARD, entries),
                    "ping": {
                        entry.filename: entry_state_to_bool(entry.state)
                        for entry in entries
                    },
                },
            }
        )

    def _subscribe_to_events(self) -> None:
        """Subscribe to dashboard events."""
        async_add_listener = DASHBOARD.bus.async_add_listener
        # Subscribe to all events
        self._event_listeners = [
            async_add_listener(
                DashboardEvent.ENTRY_STATE_CHANGED, self._on_entry_state_changed
            ),
            async_add_listener(
                DashboardEvent.ENTRY_ADDED,
                self._make_entry_handler(DashboardEvent.ENTRY_ADDED),
            ),
            async_add_listener(
                DashboardEvent.ENTRY_REMOVED,
                self._make_entry_handler(DashboardEvent.ENTRY_REMOVED),
            ),
            async_add_listener(
                DashboardEvent.ENTRY_UPDATED,
                self._make_entry_handler(DashboardEvent.ENTRY_UPDATED),
            ),
            async_add_listener(
                DashboardEvent.IMPORTABLE_DEVICE_ADDED, self._on_importable_added
            ),
            async_add_listener(
                DashboardEvent.IMPORTABLE_DEVICE_REMOVED,
                self._on_importable_removed,
            ),
        ]

    def _on_entry_state_changed(self, event: Event) -> None:
        """Handle entry state change event."""
        entry = event.data["entry"]
        state = event.data["state"]
        self._safe_send_message(
            {
                "event": DashboardEvent.ENTRY_STATE_CHANGED,
                "data": {
                    "filename": entry.filename,
                    "name": entry.name,
                    "state": entry_state_to_bool(state),
                },
            }
        )

    def _make_entry_handler(
        self, event_type: DashboardEvent
    ) -> Callable[[Event], None]:
        """Create an entry event handler."""

        def handler(event: Event) -> None:
            self._safe_send_message(
                {"event": event_type, "data": {"device": event.data["entry"].to_dict()}}
            )

        return handler

    def _on_importable_added(self, event: Event) -> None:
        """Handle importable device added event."""
        # Don't send if device is already configured
        device_name = event.data.get("device", {}).get("name")
        if device_name and DASHBOARD.entries.get_by_name(device_name):
            return
        self._safe_send_message(
            {"event": DashboardEvent.IMPORTABLE_DEVICE_ADDED, "data": event.data}
        )

    def _on_importable_removed(self, event: Event) -> None:
        """Handle importable device removed event."""
        self._safe_send_message(
            {"event": DashboardEvent.IMPORTABLE_DEVICE_REMOVED, "data": event.data}
        )

    def _safe_send_message(self, message: dict[str, Any]) -> None:
        """Send a message to the WebSocket client, ignoring closed errors."""
        with contextlib.suppress(tornado.websocket.WebSocketClosedError):
            self.write_message(json.dumps(message))

    def on_message(self, message: str) -> None:
        """Handle incoming WebSocket messages."""
        _LOGGER.debug("WebSocket received message: %s", message)
        try:
            data = json.loads(message)
        except json.JSONDecodeError as err:
            _LOGGER.debug("Failed to parse WebSocket message: %s", err)
            return

        event = data.get("event")
        _LOGGER.debug("WebSocket message event: %s", event)
        if event == DashboardEvent.PING:
            # Send pong response for client ping
            _LOGGER.debug("Received client ping, sending pong")
            self._safe_send_message({"event": DashboardEvent.PONG})
        elif event == DashboardEvent.REFRESH:
            # Signal the polling loop to refresh immediately
            _LOGGER.debug("Received refresh request, signaling polling loop")
            DASHBOARD_SUBSCRIBER.request_refresh()

    def on_close(self) -> None:
        """Handle WebSocket close."""
        # Unsubscribe from dashboard updates
        if self._dashboard_unsubscribe:
            self._dashboard_unsubscribe()
            self._dashboard_unsubscribe = None

        # Unsubscribe from events
        for remove_listener in self._event_listeners or []:
            remove_listener()

        _LOGGER.debug("Dashboard status WebSocket closed")


class SerialPortRequestHandler(BaseHandler):
    @authenticated
    async def get(self) -> None:
        ports = await asyncio.get_running_loop().run_in_executor(None, get_serial_ports)
        data = []
        for port in ports:
            desc = port.description
            if port.path == "/dev/ttyAMA0":
                desc = "UART pins on GPIO header"
            split_desc = desc.split(" - ")
            if len(split_desc) == 2 and split_desc[0] == split_desc[1]:
                # Some serial ports repeat their values
                desc = split_desc[0]
            data.append({"port": port.path, "desc": desc})
        data.append({"port": "OTA", "desc": "Over-The-Air"})
        data.sort(key=lambda x: x["port"], reverse=True)
        self.set_header("content-type", "application/json")
        self.write(json.dumps(data))


class WizardRequestHandler(BaseHandler):
    @authenticated
    def post(self) -> None:
        from esphome import wizard

        kwargs = {
            k: v
            for k, v in json.loads(self.request.body.decode()).items()
            if k
            in (
                "type",
                "name",
                "platform",
                "board",
                "ssid",
                "psk",
                "password",
                "file_content",
            )
        }
        if not kwargs["name"]:
            self.set_status(422)
            self.set_header("content-type", "application/json")
            self.write(json.dumps({"error": "Name is required"}))
            return

        if "type" not in kwargs:
            # Default to basic wizard type for backwards compatibility
            kwargs["type"] = "basic"

        kwargs["friendly_name"] = kwargs["name"]
        kwargs["name"] = friendly_name_slugify(kwargs["friendly_name"])
        if kwargs["type"] == "basic":
            kwargs["ota_password"] = secrets.token_hex(16)
            noise_psk = secrets.token_bytes(32)
            kwargs["api_encryption_key"] = base64.b64encode(noise_psk).decode()
        elif kwargs["type"] == "upload":
            try:
                kwargs["file_text"] = base64.b64decode(kwargs["file_content"]).decode(
                    "utf-8"
                )
            except (binascii.Error, UnicodeDecodeError):
                self.set_status(422)
                self.set_header("content-type", "application/json")
                self.write(
                    json.dumps({"error": "The uploaded file is not correctly encoded."})
                )
                return
        elif kwargs["type"] != "empty":
            self.set_status(422)
            self.set_header("content-type", "application/json")
            self.write(
                json.dumps(
                    {"error": f"Invalid wizard type specified: {kwargs['type']}"}
                )
            )
            return
        filename = f"{kwargs['name']}.yaml"
        destination = settings.rel_path(filename)

        # Check if destination file already exists
        if destination.exists():
            self.set_status(409)  # Conflict status code
            self.set_header("content-type", "application/json")
            self.write(
                json.dumps({"error": f"Configuration file '{filename}' already exists"})
            )
            self.finish()
            return

        success = wizard.wizard_write(path=destination, **kwargs)
        if success:
            self.set_status(200)
            self.set_header("content-type", "application/json")
            self.write(json.dumps({"configuration": filename}))
            self.finish()
        else:
            self.set_status(500)
            self.set_header("content-type", "application/json")
            self.write(
                json.dumps(
                    {"error": "Failed to write configuration, see logs for details"}
                )
            )
            self.finish()


class ImportRequestHandler(BaseHandler):
    @authenticated
    def post(self) -> None:
        from esphome.components.dashboard_import import import_config

        dashboard = DASHBOARD
        args = json.loads(self.request.body.decode())
        try:
            name = args["name"]
            friendly_name = args.get("friendly_name")
            encryption = args.get("encryption", False)

            imported_device = next(
                (
                    res
                    for res in dashboard.import_result.values()
                    if res.device_name == name
                ),
                None,
            )

            if imported_device is not None:
                network = imported_device.network
                if friendly_name is None:
                    friendly_name = imported_device.friendly_name
            else:
                network = const.CONF_WIFI

            import_config(
                settings.rel_path(f"{name}.yaml"),
                name,
                friendly_name,
                args["project_name"],
                args["package_import_url"],
                network,
                encryption,
            )
            # Make sure the device gets marked online right away
            dashboard.ping_request.set()
        except FileExistsError:
            self.set_status(500)
            self.write("File already exists")
            return
        except ValueError as e:
            _LOGGER.error(e)
            self.set_status(422)
            self.write("Invalid package url")
            return

        self.set_status(200)
        self.set_header("content-type", "application/json")
        self.write(json.dumps({"configuration": f"{name}.yaml"}))
        self.finish()


class IgnoreDeviceRequestHandler(BaseHandler):
    @authenticated
    async def post(self) -> None:
        dashboard = DASHBOARD
        try:
            args = json.loads(self.request.body.decode())
            device_name = args["name"]
            ignore = args["ignore"]
        except (json.JSONDecodeError, KeyError):
            self.set_status(400)
            self.set_header("content-type", "application/json")
            self.write(json.dumps({"error": "Invalid payload"}))
            return

        ignored_device = next(
            (
                res
                for res in dashboard.import_result.values()
                if res.device_name == device_name
            ),
            None,
        )

        if ignored_device is None:
            self.set_status(404)
            self.set_header("content-type", "application/json")
            self.write(json.dumps({"error": "Device not found"}))
            return

        if ignore:
            dashboard.ignored_devices.add(ignored_device.device_name)
        else:
            dashboard.ignored_devices.discard(ignored_device.device_name)

        loop = asyncio.get_running_loop()
        await loop.run_in_executor(None, dashboard.save_ignored_devices)

        self.set_status(204)
        self.finish()


class DownloadListRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    async def get(self, configuration: str | None = None) -> None:
        loop = asyncio.get_running_loop()
        try:
            downloads_json = await loop.run_in_executor(None, self._get, configuration)
        except vol.Invalid as exc:
            _LOGGER.exception("Error while fetching downloads", exc_info=exc)
            self.send_error(404)
            return
        if downloads_json is None:
            _LOGGER.error("Configuration %s not found", configuration)
            self.send_error(404)
            return
        self.set_status(200)
        self.set_header("content-type", "application/json")
        self.write(downloads_json)
        self.finish()

    def _get(self, configuration: str | None = None) -> dict[str, Any] | None:
        storage_path = ext_storage_path(configuration)
        storage_json = StorageJSON.load(storage_path)
        if storage_json is None:
            return None

        try:
            config = yaml_util.load_yaml(settings.rel_path(configuration))

            if const.CONF_EXTERNAL_COMPONENTS in config:
                from esphome.components.external_components import (
                    do_external_components_pass,
                )

                do_external_components_pass(config)
        except vol.Invalid:
            _LOGGER.info("Could not parse `external_components`, skipping")

        from esphome.components.esp32 import VARIANTS as ESP32_VARIANTS

        downloads: list[dict[str, Any]] = []
        platform: str = storage_json.target_platform.lower()

        if platform.upper() in ESP32_VARIANTS:
            platform = "esp32"
        elif platform in (
            const.PLATFORM_RTL87XX,
            const.PLATFORM_BK72XX,
            const.PLATFORM_LN882X,
        ):
            platform = "libretiny"

        try:
            module = importlib.import_module(f"esphome.components.{platform}")
            get_download_types = getattr(module, "get_download_types")
        except AttributeError as exc:
            raise ValueError(f"Unknown platform {platform}") from exc
        downloads = get_download_types(storage_json)
        return json.dumps(downloads)


class DownloadBinaryRequestHandler(BaseHandler):
    def _load_file(self, path: str, compressed: bool) -> bytes:
        """Load a file from disk and compress it if requested."""
        with open(path, "rb") as f:
            data = f.read()
            if compressed:
                return gzip.compress(data, 9)
            return data

    @authenticated
    @bind_config
    async def get(self, configuration: str | None = None) -> None:
        """Download a binary file."""
        loop = asyncio.get_running_loop()
        compressed = self.get_argument("compressed", "0") == "1"

        storage_path = ext_storage_path(configuration)
        storage_json = StorageJSON.load(storage_path)
        if storage_json is None:
            self.send_error(404)
            return

        # fallback to type=, but prioritize file=
        file_name = self.get_argument("type", None)
        file_name = self.get_argument("file", file_name)
        if file_name is None:
            self.send_error(400)
            return
        file_name = file_name.replace("..", "").lstrip("/")
        # get requested download name, or build it based on filename
        download_name = self.get_argument(
            "download",
            f"{storage_json.name}-{file_name}",
        )

        path = storage_json.firmware_bin_path.parent.joinpath(file_name)

        if not path.is_file():
            args = ["esphome", "idedata", settings.rel_path(configuration)]
            rc, stdout, _ = await async_run_system_command(args)

            if rc != 0:
                self.send_error(404 if rc == 2 else 500)
                return

            idedata = platformio_api.IDEData(json.loads(stdout))

            found = False
            for image in idedata.extra_flash_images:
                if image.path.endswith(file_name):
                    path = image.path
                    download_name = file_name
                    found = True
                    break

            if not found:
                self.send_error(404)
                return

        download_name = download_name + ".gz" if compressed else download_name

        self.set_header("Content-Type", "application/octet-stream")
        self.set_header(
            "Content-Disposition", f'attachment; filename="{download_name}"'
        )
        self.set_header("Cache-Control", "no-cache")
        if not Path(path).is_file():
            self.send_error(404)
            return

        data = await loop.run_in_executor(None, self._load_file, path, compressed)
        self.write(data)

        self.finish()


class EsphomeVersionHandler(BaseHandler):
    @authenticated
    def get(self) -> None:
        self.set_header("Content-Type", "application/json")
        self.write(json.dumps({"version": const.__version__}))
        self.finish()


class ListDevicesHandler(BaseHandler):
    @authenticated
    async def get(self) -> None:
        dashboard = DASHBOARD
        await dashboard.entries.async_request_update_entries()
        entries = dashboard.entries.async_all()
        self.set_header("content-type", "application/json")
        self.write(json.dumps(build_device_list_response(dashboard, entries)))


class MainRequestHandler(BaseHandler):
    @authenticated
    def get(self) -> None:
        begin = bool(self.get_argument("begin", False))
        if settings.using_password:
            # Simply accessing the xsrf_token sets the cookie for us
            self.xsrf_token  # pylint: disable=pointless-statement
        else:
            self.clear_cookie("_xsrf")

        self.render(
            "index.template.html",
            begin=begin,
            **template_args(),
            login_enabled=settings.using_password,
        )


class PrometheusServiceDiscoveryHandler(BaseHandler):
    @authenticated
    async def get(self) -> None:
        dashboard = DASHBOARD
        await dashboard.entries.async_request_update_entries()
        entries = dashboard.entries.async_all()
        self.set_header("content-type", "application/json")
        sd = []
        for entry in entries:
            if entry.web_port is None:
                continue
            labels = {
                "__meta_name": entry.name,
                "__meta_esp_platform": entry.target_platform,
                "__meta_esphome_version": entry.storage.esphome_version,
            }
            for integration in entry.storage.loaded_integrations:
                labels[f"__meta_integration_{integration}"] = "true"
            sd.append(
                {
                    "targets": [
                        f"{entry.address}:{entry.web_port}",
                    ],
                    "labels": labels,
                }
            )
        self.write(json.dumps(sd))


class BoardsRequestHandler(BaseHandler):
    @authenticated
    def get(self, platform: str) -> None:
        # filter all ESP32 variants by requested platform
        if platform.startswith("esp32"):
            from esphome.components.esp32.boards import BOARDS as ESP32_BOARDS

            boards = {
                k: v
                for k, v in ESP32_BOARDS.items()
                if v[const.KEY_VARIANT] == platform.upper()
            }
        elif platform == const.PLATFORM_ESP8266:
            from esphome.components.esp8266.boards import BOARDS as ESP8266_BOARDS

            boards = ESP8266_BOARDS
        elif platform == const.PLATFORM_RP2040:
            from esphome.components.rp2040.boards import BOARDS as RP2040_BOARDS

            boards = RP2040_BOARDS
        elif platform == const.PLATFORM_BK72XX:
            from esphome.components.bk72xx.boards import BOARDS as BK72XX_BOARDS

            boards = BK72XX_BOARDS
        elif platform == const.PLATFORM_LN882X:
            from esphome.components.ln882x.boards import BOARDS as LN882X_BOARDS

            boards = LN882X_BOARDS
        elif platform == const.PLATFORM_RTL87XX:
            from esphome.components.rtl87xx.boards import BOARDS as RTL87XX_BOARDS

            boards = RTL87XX_BOARDS
        else:
            raise ValueError(f"Unknown platform {platform}")

        # map to a {board_name: board_title} dict
        platform_boards = {key: val[const.KEY_NAME] for key, val in boards.items()}
        # sort by board title
        boards_items = sorted(platform_boards.items(), key=lambda item: item[1])
        output = [{"items": dict(boards_items)}]

        self.set_header("content-type", "application/json")
        self.write(json.dumps(output))


class PingRequestHandler(BaseHandler):
    @authenticated
    def get(self) -> None:
        dashboard = DASHBOARD
        dashboard.ping_request.set()
        if settings.status_use_mqtt:
            dashboard.mqtt_ping_request.set()
        self.set_header("content-type", "application/json")

        self.write(
            json.dumps(
                {
                    entry.filename: entry_state_to_bool(entry.state)
                    for entry in dashboard.entries.async_all()
                }
            )
        )


class InfoRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    async def get(self, configuration: str | None = None) -> None:
        yaml_path = settings.rel_path(configuration)
        dashboard = DASHBOARD
        entry = dashboard.entries.get(yaml_path)

        if not entry or entry.storage is None:
            self.set_status(404)
            return

        self.set_header("content-type", "application/json")
        self.write(entry.storage.to_json())


class EditRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    async def get(self, configuration: str | None = None) -> None:
        """Get the content of a file."""
        if not configuration.endswith((".yaml", ".yml")):
            self.send_error(404)
            return

        filename = settings.rel_path(configuration)
        if filename.resolve().parent != settings.absolute_config_dir:
            self.send_error(404)
            return

        loop = asyncio.get_running_loop()
        content = await loop.run_in_executor(
            None, self._read_file, filename, configuration
        )
        if content is not None:
            self.set_header("Content-Type", "application/yaml")
            self.write(content)

    def _read_file(self, filename: str, configuration: str) -> bytes | None:
        """Read a file and return the content as bytes."""
        try:
            with open(file=filename, encoding="utf-8") as f:
                return f.read()
        except FileNotFoundError:
            if configuration in const.SECRETS_FILES:
                return ""
            self.set_status(404)
            return None

    @authenticated
    @bind_config
    async def post(self, configuration: str | None = None) -> None:
        """Write the content of a file."""
        if not configuration.endswith((".yaml", ".yml")):
            self.send_error(404)
            return

        filename = settings.rel_path(configuration)
        if filename.resolve().parent != settings.absolute_config_dir:
            self.send_error(404)
            return

        loop = asyncio.get_running_loop()
        await loop.run_in_executor(None, write_file, filename, self.request.body)
        # Ensure the StorageJSON is updated as well
        DASHBOARD.entries.async_schedule_storage_json_update(filename)
        self.set_status(200)


class ArchiveRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def post(self, configuration: str | None = None) -> None:
        config_file = settings.rel_path(configuration)
        storage_path = ext_storage_path(configuration)

        archive_path = archive_storage_path()
        mkdir_p(archive_path)
        shutil.move(config_file, archive_path / configuration)

        storage_json = StorageJSON.load(storage_path)
        if storage_json is not None and storage_json.build_path:
            # Delete build folder (if exists)
            shutil.rmtree(storage_json.build_path, ignore_errors=True)


class UnArchiveRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def post(self, configuration: str | None = None) -> None:
        config_file = settings.rel_path(configuration)
        archive_path = archive_storage_path()
        shutil.move(archive_path / configuration, config_file)


class LoginHandler(BaseHandler):
    def get(self) -> None:
        if is_authenticated(self):
            self.redirect("./")
        else:
            self.render_login_page()

    def render_login_page(self, error: str | None = None) -> None:
        self.render(
            "login.template.html",
            error=error,
            ha_addon=settings.using_ha_addon_auth,
            has_username=bool(settings.username),
            **template_args(),
        )

    def _make_supervisor_auth_request(self) -> Response:
        """Make a request to the supervisor auth endpoint."""
        import requests

        headers = {"X-Supervisor-Token": os.getenv("SUPERVISOR_TOKEN")}
        data = {
            "username": self.get_argument("username", ""),
            "password": self.get_argument("password", ""),
        }
        return requests.post(
            "http://supervisor/auth", headers=headers, json=data, timeout=30
        )

    async def post_ha_addon_login(self) -> None:
        loop = asyncio.get_running_loop()
        try:
            req = await loop.run_in_executor(None, self._make_supervisor_auth_request)
        except Exception as err:  # pylint: disable=broad-except
            _LOGGER.warning("Error during Hass.io auth request: %s", err)
            self.set_status(500)
            self.render_login_page(error="Internal server error")
            return

        if req.status_code == 200:
            self._set_authenticated()
            self.redirect("/")
            return
        self.set_status(401)
        self.render_login_page(error="Invalid username or password")

    def _set_authenticated(self) -> None:
        """Set the authenticated cookie."""
        self.set_secure_cookie(AUTH_COOKIE_NAME, COOKIE_AUTHENTICATED_YES)

    def post_native_login(self) -> None:
        username = self.get_argument("username", "")
        password = self.get_argument("password", "")
        if settings.check_password(username, password):
            self._set_authenticated()
            self.redirect("./")
            return
        error_str = (
            "Invalid username or password" if settings.username else "Invalid password"
        )
        self.set_status(401)
        self.render_login_page(error=error_str)

    async def post(self):
        if settings.using_ha_addon_auth:
            await self.post_ha_addon_login()
        else:
            self.post_native_login()


class LogoutHandler(BaseHandler):
    @authenticated
    def get(self) -> None:
        self.clear_cookie(AUTH_COOKIE_NAME)
        self.redirect("./login")


class SecretKeysRequestHandler(BaseHandler):
    @authenticated
    def get(self) -> None:
        filename = None

        for secret_filename in const.SECRETS_FILES:
            relative_filename = settings.rel_path(secret_filename)
            if relative_filename.is_file():
                filename = relative_filename
                break

        if filename is None:
            self.send_error(404)
            return

        secret_keys = list(yaml_util.load_yaml(filename, clear_secrets=False))

        self.set_header("content-type", "application/json")
        self.write(json.dumps(secret_keys))


class SafeLoaderIgnoreUnknown(FastestAvailableSafeLoader):
    def ignore_unknown(self, node: Node) -> str:
        return f"{node.tag} {node.value}"

    def construct_yaml_binary(self, node: Node) -> str:
        return super().construct_yaml_binary(node).decode("ascii")


SafeLoaderIgnoreUnknown.add_constructor(None, SafeLoaderIgnoreUnknown.ignore_unknown)
SafeLoaderIgnoreUnknown.add_constructor(
    "tag:yaml.org,2002:binary", SafeLoaderIgnoreUnknown.construct_yaml_binary
)


class JsonConfigRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    async def get(self, configuration: str | None = None) -> None:
        filename = settings.rel_path(configuration)
        if not filename.is_file():
            self.send_error(404)
            return

        args = ["esphome", "config", str(filename), "--show-secrets"]

        rc, stdout, stderr = await async_run_system_command(args)

        if rc != 0:
            self.set_status(422)
            self.write(stderr)
            return

        data = yaml.load(stdout, Loader=SafeLoaderIgnoreUnknown)
        self.set_header("content-type", "application/json")
        self.write(json.dumps(data))
        self.finish()


def get_base_frontend_path() -> Path:
    if ENV_DEV not in os.environ:
        import esphome_dashboard

        return esphome_dashboard.where()

    static_path = os.environ[ENV_DEV]
    if not static_path.endswith("/"):
        static_path += "/"

    # This path can be relative, so resolve against the root or else templates don't work
    path = Path(os.getcwd()) / static_path / "esphome_dashboard"
    return path.resolve()


def get_static_path(*args: Iterable[str]) -> Path:
    return get_base_frontend_path() / "static" / Path(*args)


@functools.cache
def get_static_file_url(name: str) -> str:
    base = f"./static/{name}"

    if ENV_DEV in os.environ:
        return base

    # Module imports can't deduplicate if stuff added to url
    if name == "js/esphome/index.js":
        import esphome_dashboard

        return base.replace("index.js", esphome_dashboard.entrypoint())

    path = get_static_path(name)
    hash_ = hashlib.md5(path.read_bytes()).hexdigest()[:8]
    return f"{base}?hash={hash_}"


def make_app(debug=get_bool_env(ENV_DEV)) -> tornado.web.Application:
    def log_function(handler: tornado.web.RequestHandler) -> None:
        if handler.get_status() < 400:
            log_method = access_log.info

            if isinstance(handler, SerialPortRequestHandler) and not debug:
                return
            if isinstance(handler, PingRequestHandler) and not debug:
                return
        elif handler.get_status() < 500:
            log_method = access_log.warning
        else:
            log_method = access_log.error

        request_time = 1000.0 * handler.request.request_time()
        # pylint: disable=protected-access
        log_method(
            "%d %s %.2fms",
            handler.get_status(),
            handler._request_summary(),
            request_time,
        )

    class StaticFileHandler(tornado.web.StaticFileHandler):
        def get_cache_time(
            self, path: str, modified: datetime.datetime | None, mime_type: str
        ) -> int:
            """Override to customize cache control behavior."""
            if debug:
                return 0
            # Assets that are hashed have ?hash= in the URL, all javascript
            # filenames hashed so we can cache them for a long time
            if "hash" in self.request.arguments or "/javascript" in mime_type:
                return self.CACHE_MAX_AGE
            return super().get_cache_time(path, modified, mime_type)

    app_settings = {
        "debug": debug,
        "cookie_secret": settings.cookie_secret,
        "log_function": log_function,
        "websocket_ping_interval": 30.0,
        "template_path": get_base_frontend_path(),
        "xsrf_cookies": settings.using_password,
    }
    rel = settings.relative_url
    return tornado.web.Application(
        [
            (f"{rel}", MainRequestHandler),
            (f"{rel}login", LoginHandler),
            (f"{rel}logout", LogoutHandler),
            (f"{rel}logs", EsphomeLogsHandler),
            (f"{rel}upload", EsphomeUploadHandler),
            (f"{rel}run", EsphomeRunHandler),
            (f"{rel}compile", EsphomeCompileHandler),
            (f"{rel}validate", EsphomeValidateHandler),
            (f"{rel}clean-mqtt", EsphomeCleanMqttHandler),
            (f"{rel}clean-all", EsphomeCleanAllHandler),
            (f"{rel}clean", EsphomeCleanHandler),
            (f"{rel}vscode", EsphomeVscodeHandler),
            (f"{rel}ace", EsphomeAceEditorHandler),
            (f"{rel}update-all", EsphomeUpdateAllHandler),
            (f"{rel}info", InfoRequestHandler),
            (f"{rel}edit", EditRequestHandler),
            (f"{rel}downloads", DownloadListRequestHandler),
            (f"{rel}download.bin", DownloadBinaryRequestHandler),
            (f"{rel}serial-ports", SerialPortRequestHandler),
            (f"{rel}ping", PingRequestHandler),
            (f"{rel}delete", ArchiveRequestHandler),
            (f"{rel}undo-delete", UnArchiveRequestHandler),
            (f"{rel}archive", ArchiveRequestHandler),
            (f"{rel}unarchive", UnArchiveRequestHandler),
            (f"{rel}wizard", WizardRequestHandler),
            (f"{rel}static/(.*)", StaticFileHandler, {"path": get_static_path()}),
            (f"{rel}devices", ListDevicesHandler),
            (f"{rel}events", DashboardEventsWebSocket),
            (f"{rel}import", ImportRequestHandler),
            (f"{rel}secret_keys", SecretKeysRequestHandler),
            (f"{rel}json-config", JsonConfigRequestHandler),
            (f"{rel}rename", EsphomeRenameHandler),
            (f"{rel}prometheus-sd", PrometheusServiceDiscoveryHandler),
            (f"{rel}boards/([a-z0-9]+)", BoardsRequestHandler),
            (f"{rel}version", EsphomeVersionHandler),
            (f"{rel}ignore-device", IgnoreDeviceRequestHandler),
        ],
        **app_settings,
    )


def start_web_server(
    app: tornado.web.Application,
    socket: str | None,
    address: str | None,
    port: int | None,
    config_dir: str,
) -> None:
    """Start the web server listener."""

    trash_path = trash_storage_path()
    if trash_path.is_dir() and trash_path.exists():
        _LOGGER.info("Renaming 'trash' folder to 'archive'")
        archive_path = archive_storage_path()
        shutil.move(trash_path, archive_path)

    if socket is None:
        _LOGGER.info(
            "Starting dashboard web server on http://%s:%s and configuration dir %s...",
            address,
            port,
            config_dir,
        )
        app.listen(port, address)
        return

    _LOGGER.info(
        "Starting dashboard web server on unix socket %s and configuration dir %s...",
        socket,
        config_dir,
    )
    server = tornado.httpserver.HTTPServer(app)
    socket = tornado.netutil.bind_unix_socket(socket, mode=0o666)
    server.add_socket(socket)
