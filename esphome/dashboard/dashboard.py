import base64
import codecs
import collections
import functools
import hashlib
import hmac
import json
import logging
import multiprocessing
import os
import secrets
import shutil
import subprocess
import threading
from pathlib import Path
from typing import Optional

import tornado
import tornado.concurrent
import tornado.gen
import tornado.httpserver
import tornado.ioloop
import tornado.iostream
import tornado.netutil
import tornado.process
import tornado.web
import tornado.websocket
import yaml
from tornado.log import access_log

from esphome import const, platformio_api, util, yaml_util
from esphome.helpers import get_bool_env, mkdir_p, run_system_command
from esphome.storage_json import (
    EsphomeStorageJSON,
    StorageJSON,
    esphome_storage_path,
    ext_storage_path,
    trash_storage_path,
)
from esphome.util import get_serial_ports, shlex_quote
from esphome.zeroconf import DashboardImportDiscovery, DashboardStatus, EsphomeZeroconf

from .util import friendly_name_slugify, password_hash

_LOGGER = logging.getLogger(__name__)

ENV_DEV = "ESPHOME_DASHBOARD_DEV"


class DashboardSettings:
    def __init__(self):
        self.config_dir = ""
        self.password_hash = ""
        self.username = ""
        self.using_password = False
        self.on_ha_addon = False
        self.cookie_secret = None
        self.absolute_config_dir = None

    def parse_args(self, args):
        self.on_ha_addon = args.ha_addon
        password = args.password or os.getenv("PASSWORD", "")
        if not self.on_ha_addon:
            self.username = args.username or os.getenv("USERNAME", "")
            self.using_password = bool(password)
        if self.using_password:
            self.password_hash = password_hash(password)
        self.config_dir = args.configuration
        self.absolute_config_dir = Path(self.config_dir).resolve()

    @property
    def relative_url(self):
        return os.getenv("ESPHOME_DASHBOARD_RELATIVE_URL", "/")

    @property
    def status_use_ping(self):
        return get_bool_env("ESPHOME_DASHBOARD_USE_PING")

    @property
    def using_ha_addon_auth(self):
        if not self.on_ha_addon:
            return False
        return not get_bool_env("DISABLE_HA_AUTHENTICATION")

    @property
    def using_auth(self):
        return self.using_password or self.using_ha_addon_auth

    def check_password(self, username, password):
        if not self.using_auth:
            return True
        if username != self.username:
            return False

        # Compare password in constant running time (to prevent timing attacks)
        return hmac.compare_digest(self.password_hash, password_hash(password))

    def rel_path(self, *args):
        joined_path = os.path.join(self.config_dir, *args)
        # Raises ValueError if not relative to ESPHome config folder
        Path(joined_path).resolve().relative_to(self.absolute_config_dir)
        return joined_path

    def list_yaml_files(self):
        return util.list_yaml_files([self.config_dir])


settings = DashboardSettings()

cookie_authenticated_yes = b"yes"


def template_args():
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
        "streamer_mode": get_bool_env("ESPHOME_STREAMER_MODE"),
        "config_dir": settings.config_dir,
    }


def authenticated(func):
    @functools.wraps(func)
    def decorator(self, *args, **kwargs):
        if not is_authenticated(self):
            self.redirect("./login")
            return None
        return func(self, *args, **kwargs)

    return decorator


def is_authenticated(request_handler):
    if settings.on_ha_addon:
        # Handle ingress - disable auth on ingress port
        # X-HA-Ingress is automatically stripped on the non-ingress server in nginx
        header = request_handler.request.headers.get("X-HA-Ingress", "NO")
        if str(header) == "YES":
            return True
    if settings.using_auth:
        return (
            request_handler.get_secure_cookie("authenticated")
            == cookie_authenticated_yes
        )
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

    for _, method in cls.__dict__.items():
        if hasattr(method, "_message_handler"):
            cls._message_handlers[method._message_handler] = method

    return cls


def websocket_method(name):
    def wrap(fn):
        # pylint: disable=protected-access
        fn._message_handler = name
        return fn

    return wrap


@websocket_class
class EsphomeCommandWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self, application, request, **kwargs):
        super().__init__(application, request, **kwargs)
        self._proc = None
        self._is_closed = False

    @authenticated
    def on_message(self, message):
        # Messages are always JSON, 500 when not
        json_message = json.loads(message)
        type_ = json_message["type"]
        # pylint: disable=no-member
        handlers = type(self)._message_handlers
        if type_ not in handlers:
            _LOGGER.warning("Requested unknown message type %s", type_)
            return

        handlers[type_](self, json_message)

    @websocket_method("spawn")
    def handle_spawn(self, json_message):
        if self._proc is not None:
            # spawn can only be called once
            return
        command = self.build_command(json_message)
        _LOGGER.info("Running command '%s'", " ".join(shlex_quote(x) for x in command))
        self._proc = tornado.process.Subprocess(
            command,
            stdout=tornado.process.Subprocess.STREAM,
            stderr=subprocess.STDOUT,
            stdin=tornado.process.Subprocess.STREAM,
        )
        self._proc.set_exit_callback(self._proc_on_exit)
        tornado.ioloop.IOLoop.current().spawn_callback(self._redirect_stdout)

    @property
    def is_process_active(self):
        return self._proc is not None and self._proc.returncode is None

    @websocket_method("stdin")
    def handle_stdin(self, json_message):
        if not self.is_process_active:
            return
        data = json_message["data"]
        data = codecs.encode(data, "utf8", "replace")
        _LOGGER.debug("< stdin: %s", data)
        self._proc.stdin.write(data)

    @tornado.gen.coroutine
    def _redirect_stdout(self):
        reg = b"[\n\r]"

        while True:
            try:
                data = yield self._proc.stdout.read_until_regex(reg)
            except tornado.iostream.StreamClosedError:
                break
            data = codecs.decode(data, "utf8", "replace")

            _LOGGER.debug("> stdout: %s", data)
            self.write_message({"event": "line", "data": data})

    def _proc_on_exit(self, returncode):
        if not self._is_closed:
            # Check if the proc was not forcibly closed
            _LOGGER.info("Process exited with return code %s", returncode)
            self.write_message({"event": "exit", "code": returncode})

    def on_close(self):
        # Check if proc exists (if 'start' has been run)
        if self.is_process_active:
            _LOGGER.debug("Terminating process")
            self._proc.proc.terminate()
        # Shutdown proc on WS close
        self._is_closed = True

    def build_command(self, json_message):
        raise NotImplementedError


class EsphomeLogsHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message["configuration"])
        return [
            "esphome",
            "--dashboard",
            "logs",
            config_file,
            "--device",
            json_message["port"],
        ]


class EsphomeRenameHandler(EsphomeCommandWebSocket):
    old_name: str

    def build_command(self, json_message):
        config_file = settings.rel_path(json_message["configuration"])
        self.old_name = json_message["configuration"]
        return [
            "esphome",
            "--dashboard",
            "rename",
            config_file,
            json_message["newName"],
        ]

    def _proc_on_exit(self, returncode):
        super()._proc_on_exit(returncode)

        if returncode != 0:
            return

        # Remove the old ping result from the cache
        PING_RESULT.pop(self.old_name, None)


class EsphomeUploadHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message["configuration"])
        return [
            "esphome",
            "--dashboard",
            "upload",
            config_file,
            "--device",
            json_message["port"],
        ]


class EsphomeRunHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message["configuration"])
        return [
            "esphome",
            "--dashboard",
            "run",
            config_file,
            "--device",
            json_message["port"],
        ]


class EsphomeCompileHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message["configuration"])
        command = ["esphome", "--dashboard", "compile"]
        if json_message.get("only_generate", False):
            command.append("--only-generate")
        command.append(config_file)
        return command


class EsphomeValidateHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message["configuration"])
        return ["esphome", "--dashboard", "config", config_file]


class EsphomeCleanMqttHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message["configuration"])
        return ["esphome", "--dashboard", "clean-mqtt", config_file]


class EsphomeCleanHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message["configuration"])
        return ["esphome", "--dashboard", "clean", config_file]


class EsphomeVscodeHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        return ["esphome", "--dashboard", "-q", "vscode", "dummy"]


class EsphomeAceEditorHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        return ["esphome", "--dashboard", "-q", "vscode", "--ace", settings.config_dir]


class EsphomeUpdateAllHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        return ["esphome", "--dashboard", "update-all", settings.config_dir]


class SerialPortRequestHandler(BaseHandler):
    @authenticated
    def get(self):
        ports = get_serial_ports()
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
    def post(self):
        from esphome import wizard

        kwargs = {
            k: v
            for k, v in json.loads(self.request.body.decode()).items()
            if k in ("name", "platform", "board", "ssid", "psk", "password")
        }
        if not kwargs["name"]:
            self.set_status(422)
            self.set_header("content-type", "application/json")
            self.write(json.dumps({"error": "Name is required"}))
            return

        kwargs["friendly_name"] = kwargs["name"]
        kwargs["name"] = friendly_name_slugify(kwargs["friendly_name"])

        kwargs["ota_password"] = secrets.token_hex(16)
        noise_psk = secrets.token_bytes(32)
        kwargs["api_encryption_key"] = base64.b64encode(noise_psk).decode()
        filename = f"{kwargs['name']}.yaml"
        destination = settings.rel_path(filename)
        wizard.wizard_write(path=destination, **kwargs)
        self.set_status(200)
        self.set_header("content-type", "application/json")
        self.write(json.dumps({"configuration": filename}))
        self.finish()


class ImportRequestHandler(BaseHandler):
    @authenticated
    def post(self):
        from esphome.components.dashboard_import import import_config

        args = json.loads(self.request.body.decode())
        try:
            name = args["name"]
            friendly_name = args.get("friendly_name")
            encryption = args.get("encryption", False)

            imported_device = next(
                (res for res in IMPORT_RESULT.values() if res.device_name == name), None
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
        except FileExistsError:
            self.set_status(500)
            self.write("File already exists")
            return
        except ValueError:
            self.set_status(422)
            self.write("Invalid package url")
            return

        self.set_status(200)
        self.set_header("content-type", "application/json")
        self.write(json.dumps({"configuration": f"{name}.yaml"}))
        self.finish()


class DownloadBinaryRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def get(self, configuration=None):
        type = self.get_argument("type", "firmware.bin")

        storage_path = ext_storage_path(settings.config_dir, configuration)
        storage_json = StorageJSON.load(storage_path)
        if storage_json is None:
            self.send_error(404)
            return

        if storage_json.target_platform.lower() == const.PLATFORM_RP2040:
            filename = f"{storage_json.name}.uf2"
            path = storage_json.firmware_bin_path.replace(
                "firmware.bin", "firmware.uf2"
            )

        elif storage_json.target_platform.lower() == const.PLATFORM_ESP8266:
            filename = f"{storage_json.name}.bin"
            path = storage_json.firmware_bin_path

        elif type == "firmware.bin":
            filename = f"{storage_json.name}.bin"
            path = storage_json.firmware_bin_path

        elif type == "firmware-factory.bin":
            filename = f"{storage_json.name}-factory.bin"
            path = storage_json.firmware_bin_path.replace(
                "firmware.bin", "firmware-factory.bin"
            )

        else:
            args = ["esphome", "idedata", settings.rel_path(configuration)]
            rc, stdout, _ = run_system_command(*args)

            if rc != 0:
                self.send_error(404 if rc == 2 else 500)
                return

            idedata = platformio_api.IDEData(json.loads(stdout))

            found = False
            for image in idedata.extra_flash_images:
                if image.path.endswith(type):
                    path = image.path
                    filename = type
                    found = True
                    break

            if not found:
                self.send_error(404)
                return

        self.set_header("Content-Type", "application/octet-stream")
        self.set_header("Content-Disposition", f'attachment; filename="{filename}"')
        self.set_header("Cache-Control", "no-cache")
        if not Path(path).is_file():
            self.send_error(404)
            return

        with open(path, "rb") as f:
            while True:
                data = f.read(16384)
                if not data:
                    break
                self.write(data)
        self.finish()


def _list_dashboard_entries():
    files = settings.list_yaml_files()
    return [DashboardEntry(file) for file in files]


class DashboardEntry:
    def __init__(self, path):
        self.path = path
        self._storage = None
        self._loaded_storage = False

    @property
    def filename(self):
        return os.path.basename(self.path)

    @property
    def storage(self) -> Optional[StorageJSON]:
        if not self._loaded_storage:
            self._storage = StorageJSON.load(
                ext_storage_path(settings.config_dir, self.filename)
            )
            self._loaded_storage = True
        return self._storage

    @property
    def address(self):
        if self.storage is None:
            return None
        return self.storage.address

    @property
    def web_port(self):
        if self.storage is None:
            return None
        return self.storage.web_port

    @property
    def name(self):
        if self.storage is None:
            return self.filename.replace(".yml", "").replace(".yaml", "")
        return self.storage.name

    @property
    def friendly_name(self):
        if self.storage is None:
            return self.name
        return self.storage.friendly_name

    @property
    def comment(self):
        if self.storage is None:
            return None
        return self.storage.comment

    @property
    def target_platform(self):
        if self.storage is None:
            return None
        return self.storage.target_platform

    @property
    def update_available(self):
        if self.storage is None:
            return True
        return self.update_old != self.update_new

    @property
    def update_old(self):
        if self.storage is None:
            return ""
        return self.storage.esphome_version or ""

    @property
    def update_new(self):
        return const.__version__

    @property
    def loaded_integrations(self):
        if self.storage is None:
            return []
        return self.storage.loaded_integrations


class ListDevicesHandler(BaseHandler):
    @authenticated
    def get(self):
        entries = _list_dashboard_entries()
        self.set_header("content-type", "application/json")
        configured = {entry.name for entry in entries}
        self.write(
            json.dumps(
                {
                    "configured": [
                        {
                            "name": entry.name,
                            "friendly_name": entry.friendly_name,
                            "configuration": entry.filename,
                            "loaded_integrations": entry.loaded_integrations,
                            "deployed_version": entry.update_old,
                            "current_version": entry.update_new,
                            "path": entry.path,
                            "comment": entry.comment,
                            "address": entry.address,
                            "web_port": entry.web_port,
                            "target_platform": entry.target_platform,
                        }
                        for entry in entries
                    ],
                    "importable": [
                        {
                            "name": res.device_name,
                            "friendly_name": res.friendly_name,
                            "package_import_url": res.package_import_url,
                            "project_name": res.project_name,
                            "project_version": res.project_version,
                            "network": res.network,
                        }
                        for res in IMPORT_RESULT.values()
                        if res.device_name not in configured
                    ],
                }
            )
        )


class MainRequestHandler(BaseHandler):
    @authenticated
    def get(self):
        begin = bool(self.get_argument("begin", False))

        self.render(
            "index.template.html",
            begin=begin,
            **template_args(),
            login_enabled=settings.using_password,
        )


def _ping_func(filename, address):
    if os.name == "nt":
        command = ["ping", "-n", "1", address]
    else:
        command = ["ping", "-c", "1", address]
    rc, _, _ = run_system_command(*command)
    return filename, rc == 0


class PrometheusServiceDiscoveryHandler(BaseHandler):
    @authenticated
    def get(self):
        entries = _list_dashboard_entries()
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
    def get(self, platform: str):
        from esphome.components.esp32.boards import BOARDS as ESP32_BOARDS
        from esphome.components.esp8266.boards import BOARDS as ESP8266_BOARDS
        from esphome.components.rp2040.boards import BOARDS as RP2040_BOARDS

        platform_to_boards = {
            "esp32": ESP32_BOARDS,
            "esp8266": ESP8266_BOARDS,
            "rp2040": RP2040_BOARDS,
        }
        # filter all ESP32 variants by requested platform
        if platform.startswith("esp32"):
            boards = {
                k: v
                for k, v in platform_to_boards["esp32"].items()
                if v[const.KEY_VARIANT] == platform.upper()
            }
        else:
            boards = platform_to_boards[platform]

        # map to a {board_name: board_title} dict
        platform_boards = {key: val[const.KEY_NAME] for key, val in boards.items()}
        # sort by board title
        boards_items = sorted(platform_boards.items(), key=lambda item: item[1])
        output = [dict(items=dict(boards_items))]

        self.set_header("content-type", "application/json")
        self.write(json.dumps(output))


class MDNSStatusThread(threading.Thread):
    def run(self):
        global IMPORT_RESULT

        zc = EsphomeZeroconf()

        def on_update(dat):
            for key, b in dat.items():
                PING_RESULT[key] = b

        stat = DashboardStatus(zc, on_update)
        imports = DashboardImportDiscovery(zc)

        stat.start()
        while not STOP_EVENT.is_set():
            entries = _list_dashboard_entries()
            stat.request_query(
                {entry.filename: f"{entry.name}.local." for entry in entries}
            )
            IMPORT_RESULT = imports.import_state

            PING_REQUEST.wait()
            PING_REQUEST.clear()

        stat.stop()
        stat.join()
        imports.cancel()
        zc.close()


class PingStatusThread(threading.Thread):
    def run(self):
        with multiprocessing.Pool(processes=8) as pool:
            while not STOP_EVENT.wait(2):
                # Only do pings if somebody has the dashboard open

                def callback(ret):
                    PING_RESULT[ret[0]] = ret[1]

                entries = _list_dashboard_entries()
                queue = collections.deque()
                for entry in entries:
                    if entry.address is None:
                        PING_RESULT[entry.filename] = None
                        continue

                    result = pool.apply_async(
                        _ping_func, (entry.filename, entry.address), callback=callback
                    )
                    queue.append(result)

                while queue:
                    item = queue[0]
                    if item.ready():
                        queue.popleft()
                        continue

                    try:
                        item.get(0.1)
                    except OSError:
                        # ping not installed
                        pass
                    except multiprocessing.TimeoutError:
                        pass

                    if STOP_EVENT.is_set():
                        pool.terminate()
                        return

                PING_REQUEST.wait()
                PING_REQUEST.clear()


class PingRequestHandler(BaseHandler):
    @authenticated
    def get(self):
        PING_REQUEST.set()
        self.set_header("content-type", "application/json")
        self.write(json.dumps(PING_RESULT))


class InfoRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def get(self, configuration=None):
        yaml_path = settings.rel_path(configuration)
        all_yaml_files = settings.list_yaml_files()

        if yaml_path not in all_yaml_files:
            self.set_status(404)
            return

        self.set_header("content-type", "application/json")
        self.write(DashboardEntry(yaml_path).storage.to_json())


class EditRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def get(self, configuration=None):
        filename = settings.rel_path(configuration)
        content = ""
        if os.path.isfile(filename):
            with open(file=filename, encoding="utf-8") as f:
                content = f.read()
        self.write(content)

    @authenticated
    @bind_config
    def post(self, configuration=None):
        with open(file=settings.rel_path(configuration), mode="wb") as f:
            f.write(self.request.body)
        self.set_status(200)


class DeleteRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def post(self, configuration=None):
        config_file = settings.rel_path(configuration)
        storage_path = ext_storage_path(settings.config_dir, configuration)

        trash_path = trash_storage_path(settings.config_dir)
        mkdir_p(trash_path)
        shutil.move(config_file, os.path.join(trash_path, configuration))

        storage_json = StorageJSON.load(storage_path)
        if storage_json is not None:
            # Delete build folder (if exists)
            name = storage_json.name
            build_folder = os.path.join(settings.config_dir, name)
            if build_folder is not None:
                shutil.rmtree(build_folder, os.path.join(trash_path, name))

        # Remove the old ping result from the cache
        PING_RESULT.pop(configuration, None)


class UndoDeleteRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def post(self, configuration=None):
        config_file = settings.rel_path(configuration)
        trash_path = trash_storage_path(settings.config_dir)
        shutil.move(os.path.join(trash_path, configuration), config_file)


PING_RESULT: dict = {}
IMPORT_RESULT = {}
STOP_EVENT = threading.Event()
PING_REQUEST = threading.Event()


class LoginHandler(BaseHandler):
    def get(self):
        if is_authenticated(self):
            self.redirect("./")
        else:
            self.render_login_page()

    def render_login_page(self, error=None):
        self.render(
            "login.template.html",
            error=error,
            ha_addon=settings.using_ha_addon_auth,
            has_username=bool(settings.username),
            **template_args(),
        )

    def post_ha_addon_login(self):
        import requests

        headers = {
            "X-Supervisor-Token": os.getenv("SUPERVISOR_TOKEN"),
        }

        data = {
            "username": self.get_argument("username", ""),
            "password": self.get_argument("password", ""),
        }
        try:
            req = requests.post(
                "http://supervisor/auth", headers=headers, json=data, timeout=30
            )
            if req.status_code == 200:
                self.set_secure_cookie("authenticated", cookie_authenticated_yes)
                self.redirect("/")
                return
        except Exception as err:  # pylint: disable=broad-except
            _LOGGER.warning("Error during Hass.io auth request: %s", err)
            self.set_status(500)
            self.render_login_page(error="Internal server error")
            return
        self.set_status(401)
        self.render_login_page(error="Invalid username or password")

    def post_native_login(self):
        username = self.get_argument("username", "")
        password = self.get_argument("password", "")
        if settings.check_password(username, password):
            self.set_secure_cookie("authenticated", cookie_authenticated_yes)
            self.redirect("./")
            return
        error_str = (
            "Invalid username or password" if settings.username else "Invalid password"
        )
        self.set_status(401)
        self.render_login_page(error=error_str)

    def post(self):
        if settings.using_ha_addon_auth:
            self.post_ha_addon_login()
        else:
            self.post_native_login()


class LogoutHandler(BaseHandler):
    @authenticated
    def get(self):
        self.clear_cookie("authenticated")
        self.redirect("./login")


class SecretKeysRequestHandler(BaseHandler):
    @authenticated
    def get(self):

        filename = None

        for secret_filename in const.SECRETS_FILES:
            relative_filename = settings.rel_path(secret_filename)
            if os.path.isfile(relative_filename):
                filename = relative_filename
                break

        if filename is None:
            self.send_error(404)
            return

        secret_keys = list(yaml_util.load_yaml(filename, clear_secrets=False))

        self.set_header("content-type", "application/json")
        self.write(json.dumps(secret_keys))


class SafeLoaderIgnoreUnknown(yaml.SafeLoader):
    def ignore_unknown(self, node):
        return f"{node.tag} {node.value}"


SafeLoaderIgnoreUnknown.add_constructor(None, SafeLoaderIgnoreUnknown.ignore_unknown)


class JsonConfigRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def get(self, configuration=None):
        filename = settings.rel_path(configuration)
        if not os.path.isfile(filename):
            self.send_error(404)
            return

        args = ["esphome", "config", settings.rel_path(configuration), "--show-secrets"]

        rc, stdout, _ = run_system_command(*args)

        if rc != 0:
            self.send_error(422)
            return

        data = yaml.load(stdout, Loader=SafeLoaderIgnoreUnknown)
        self.set_header("content-type", "application/json")
        self.write(json.dumps(data))
        self.finish()


def get_base_frontend_path():
    if ENV_DEV not in os.environ:
        import esphome_dashboard

        return esphome_dashboard.where()

    static_path = os.environ[ENV_DEV]
    if not static_path.endswith("/"):
        static_path += "/"

    # This path can be relative, so resolve against the root or else templates don't work
    return os.path.abspath(os.path.join(os.getcwd(), static_path, "esphome_dashboard"))


def get_static_path(*args):
    return os.path.join(get_base_frontend_path(), "static", *args)


@functools.cache
def get_static_file_url(name):
    base = f"./static/{name}"

    if ENV_DEV in os.environ:
        return base

    # Module imports can't deduplicate if stuff added to url
    if name == "js/esphome/index.js":
        import esphome_dashboard

        return base.replace("index.js", esphome_dashboard.entrypoint())

    path = get_static_path(name)
    with open(path, "rb") as f_handle:
        hash_ = hashlib.md5(f_handle.read()).hexdigest()[:8]
    return f"{base}?hash={hash_}"


def make_app(debug=get_bool_env(ENV_DEV)):
    def log_function(handler):
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
        def set_extra_headers(self, path):
            if "favicon.ico" in path:
                self.set_header("Cache-Control", "max-age=84600, public")
            else:
                self.set_header(
                    "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0"
                )

    app_settings = {
        "debug": debug,
        "cookie_secret": settings.cookie_secret,
        "log_function": log_function,
        "websocket_ping_interval": 30.0,
        "template_path": get_base_frontend_path(),
    }
    rel = settings.relative_url
    app = tornado.web.Application(
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
            (f"{rel}clean", EsphomeCleanHandler),
            (f"{rel}vscode", EsphomeVscodeHandler),
            (f"{rel}ace", EsphomeAceEditorHandler),
            (f"{rel}update-all", EsphomeUpdateAllHandler),
            (f"{rel}info", InfoRequestHandler),
            (f"{rel}edit", EditRequestHandler),
            (f"{rel}download.bin", DownloadBinaryRequestHandler),
            (f"{rel}serial-ports", SerialPortRequestHandler),
            (f"{rel}ping", PingRequestHandler),
            (f"{rel}delete", DeleteRequestHandler),
            (f"{rel}undo-delete", UndoDeleteRequestHandler),
            (f"{rel}wizard", WizardRequestHandler),
            (f"{rel}static/(.*)", StaticFileHandler, {"path": get_static_path()}),
            (f"{rel}devices", ListDevicesHandler),
            (f"{rel}import", ImportRequestHandler),
            (f"{rel}secret_keys", SecretKeysRequestHandler),
            (f"{rel}json-config", JsonConfigRequestHandler),
            (f"{rel}rename", EsphomeRenameHandler),
            (f"{rel}prometheus-sd", PrometheusServiceDiscoveryHandler),
            (f"{rel}boards/([a-z0-9]+)", BoardsRequestHandler),
        ],
        **app_settings,
    )

    return app


def start_web_server(args):
    settings.parse_args(args)
    mkdir_p(settings.rel_path(".esphome"))

    if settings.using_auth:
        path = esphome_storage_path(settings.config_dir)
        storage = EsphomeStorageJSON.load(path)
        if storage is None:
            storage = EsphomeStorageJSON.get_default()
            storage.save(path)
        settings.cookie_secret = storage.cookie_secret

    app = make_app(args.verbose)
    if args.socket is not None:
        _LOGGER.info(
            "Starting dashboard web server on unix socket %s and configuration dir %s...",
            args.socket,
            settings.config_dir,
        )
        server = tornado.httpserver.HTTPServer(app)
        socket = tornado.netutil.bind_unix_socket(args.socket, mode=0o666)
        server.add_socket(socket)
    else:
        _LOGGER.info(
            "Starting dashboard web server on http://%s:%s and configuration dir %s...",
            args.address,
            args.port,
            settings.config_dir,
        )
        app.listen(args.port, args.address)

        if args.open_ui:
            import webbrowser

            webbrowser.open(f"http://{args.address}:{args.port}")

    if settings.status_use_ping:
        status_thread = PingStatusThread()
    else:
        status_thread = MDNSStatusThread()
    status_thread.start()
    try:
        tornado.ioloop.IOLoop.current().start()
    except KeyboardInterrupt:
        _LOGGER.info("Shutting down...")
        STOP_EVENT.set()
        PING_REQUEST.set()
        status_thread.join()
        if args.socket is not None:
            os.remove(args.socket)
