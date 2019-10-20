# pylint: disable=wrong-import-position
from __future__ import print_function

import codecs
import collections
import functools
import hashlib
import hmac
import json
import logging
import multiprocessing
import os
import shutil
import subprocess
import threading

import tornado
import tornado.concurrent
import tornado.gen
import tornado.httpserver
import tornado.ioloop
import tornado.iostream
from tornado.log import access_log
import tornado.netutil
import tornado.process
import tornado.web
import tornado.websocket

from esphome import const, util
from esphome.__main__ import get_serial_ports
from esphome.helpers import mkdir_p, get_bool_env, run_system_command
from esphome.py_compat import IS_PY2, decode_text
from esphome.storage_json import EsphomeStorageJSON, StorageJSON, \
    esphome_storage_path, ext_storage_path, trash_storage_path
from esphome.util import shlex_quote

# pylint: disable=unused-import, wrong-import-order
from typing import Optional  # noqa

from esphome.zeroconf import DashboardStatus, Zeroconf

_LOGGER = logging.getLogger(__name__)


class DashboardSettings(object):
    def __init__(self):
        self.config_dir = ''
        self.password_digest = ''
        self.username = ''
        self.using_password = False
        self.on_hassio = False
        self.cookie_secret = None

    def parse_args(self, args):
        self.on_hassio = args.hassio
        password = args.password or os.getenv('PASSWORD', '')
        if not self.on_hassio:
            self.username = args.username or os.getenv('USERNAME', '')
            self.using_password = bool(password)
        if self.using_password:
            if IS_PY2:
                self.password_digest = hmac.new(password).digest()
            else:
                self.password_digest = hmac.new(password.encode()).digest()
        self.config_dir = args.configuration[0]

    @property
    def relative_url(self):
        return os.getenv('ESPHOME_DASHBOARD_RELATIVE_URL', '/')

    @property
    def status_use_ping(self):
        return get_bool_env('ESPHOME_DASHBOARD_USE_PING')

    @property
    def using_hassio_auth(self):
        if not self.on_hassio:
            return False
        return not get_bool_env('DISABLE_HA_AUTHENTICATION')

    @property
    def using_auth(self):
        return self.using_password or self.using_hassio_auth

    def check_password(self, username, password):
        if not self.using_auth:
            return True

        if IS_PY2:
            password = hmac.new(password).digest()
        else:
            password = hmac.new(password.encode()).digest()
        return username == self.username and hmac.compare_digest(self.password_digest, password)

    def rel_path(self, *args):
        return os.path.join(self.config_dir, *args)

    def list_yaml_files(self):
        return util.list_yaml_files(self.config_dir)


settings = DashboardSettings()

if IS_PY2:
    cookie_authenticated_yes = 'yes'
else:
    cookie_authenticated_yes = b'yes'


def template_args():
    version = const.__version__
    return {
        'version': version,
        'docs_link': 'https://beta.esphome.io/' if 'b' in version else 'https://esphome.io/',
        'get_static_file_url': get_static_file_url,
        'relative_url': settings.relative_url,
        'streamer_mode': get_bool_env('ESPHOME_STREAMER_MODE'),
        'config_dir': settings.config_dir,
    }


def authenticated(func):
    @functools.wraps(func)
    def decorator(self, *args, **kwargs):
        if not is_authenticated(self):
            self.redirect('./login')
            return None
        return func(self, *args, **kwargs)
    return decorator


def is_authenticated(request_handler):
    if settings.on_hassio:
        # Handle ingress - disable auth on ingress port
        # X-Hassio-Ingress is automatically stripped on the non-ingress server in nginx
        header = request_handler.request.headers.get('X-Hassio-Ingress', 'NO')
        if str(header) == 'YES':
            return True
    if settings.using_auth:
        return request_handler.get_secure_cookie('authenticated') == cookie_authenticated_yes
    return True


def bind_config(func):
    def decorator(self, *args, **kwargs):
        configuration = self.get_argument('configuration')
        if not is_allowed(configuration):
            self.set_status(500)
            return None
        kwargs = kwargs.copy()
        kwargs['configuration'] = configuration
        return func(self, *args, **kwargs)
    return decorator


# pylint: disable=abstract-method
class BaseHandler(tornado.web.RequestHandler):
    pass


def websocket_class(cls):
    # pylint: disable=protected-access
    if not hasattr(cls, '_message_handlers'):
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


# pylint: disable=abstract-method, arguments-differ
@websocket_class
class EsphomeCommandWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self, application, request, **kwargs):
        super(EsphomeCommandWebSocket, self).__init__(application, request, **kwargs)
        self._proc = None
        self._is_closed = False

    @authenticated
    def on_message(self, message):
        # Messages are always JSON, 500 when not
        json_message = json.loads(message)
        type_ = json_message['type']
        # pylint: disable=no-member
        handlers = type(self)._message_handlers
        if type_ not in handlers:
            _LOGGER.warning("Requested unknown message type %s", type_)
            return

        handlers[type_](self, json_message)

    @websocket_method('spawn')
    def handle_spawn(self, json_message):
        if self._proc is not None:
            # spawn can only be called once
            return
        command = self.build_command(json_message)
        _LOGGER.info(u"Running command '%s'", ' '.join(shlex_quote(x) for x in command))
        self._proc = tornado.process.Subprocess(command,
                                                stdout=tornado.process.Subprocess.STREAM,
                                                stderr=subprocess.STDOUT,
                                                stdin=tornado.process.Subprocess.STREAM)
        self._proc.set_exit_callback(self._proc_on_exit)
        tornado.ioloop.IOLoop.current().spawn_callback(self._redirect_stdout)

    @property
    def is_process_active(self):
        return self._proc is not None and self._proc.returncode is None

    @websocket_method('stdin')
    def handle_stdin(self, json_message):
        if not self.is_process_active:
            return
        data = json_message['data']
        data = codecs.encode(data, 'utf8', 'replace')
        _LOGGER.debug("< stdin: %s", data)
        self._proc.stdin.write(data)

    @tornado.gen.coroutine
    def _redirect_stdout(self):
        if IS_PY2:
            reg = '[\n\r]'
        else:
            reg = b'[\n\r]'

        while True:
            try:
                data = yield self._proc.stdout.read_until_regex(reg)
            except tornado.iostream.StreamClosedError:
                break
            data = codecs.decode(data, 'utf8', 'replace')

            _LOGGER.debug("> stdout: %s", data)
            self.write_message({'event': 'line', 'data': data})

    def _proc_on_exit(self, returncode):
        if not self._is_closed:
            # Check if the proc was not forcibly closed
            _LOGGER.info("Process exited with return code %s", returncode)
            self.write_message({'event': 'exit', 'code': returncode})

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
        config_file = settings.rel_path(json_message['configuration'])
        return ["esphome", "--dashboard", config_file, "logs", '--serial-port',
                json_message["port"]]


class EsphomeUploadHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message['configuration'])
        return ["esphome", "--dashboard", config_file, "run", '--upload-port',
                json_message["port"]]


class EsphomeCompileHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message['configuration'])
        return ["esphome", "--dashboard", config_file, "compile"]


class EsphomeValidateHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message['configuration'])
        return ["esphome", "--dashboard", config_file, "config"]


class EsphomeCleanMqttHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message['configuration'])
        return ["esphome", "--dashboard", config_file, "clean-mqtt"]


class EsphomeCleanHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        config_file = settings.rel_path(json_message['configuration'])
        return ["esphome", "--dashboard", config_file, "clean"]


class EsphomeVscodeHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        return ["esphome", "--dashboard", "-q", 'dummy', "vscode"]


class EsphomeAceEditorHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        return ["esphome", "--dashboard", "-q", settings.config_dir, "vscode", "--ace"]


class EsphomeUpdateAllHandler(EsphomeCommandWebSocket):
    def build_command(self, json_message):
        return ["esphome", "--dashboard", settings.config_dir, "update-all"]


class SerialPortRequestHandler(BaseHandler):
    @authenticated
    def get(self):
        ports = get_serial_ports()
        data = []
        for port, desc in ports:
            if port == '/dev/ttyAMA0':
                desc = 'UART pins on GPIO header'
            split_desc = desc.split(' - ')
            if len(split_desc) == 2 and split_desc[0] == split_desc[1]:
                # Some serial ports repeat their values
                desc = split_desc[0]
            data.append({'port': port, 'desc': desc})
        data.append({'port': 'OTA', 'desc': 'Over-The-Air'})
        data.sort(key=lambda x: x['port'], reverse=True)
        self.write(json.dumps(data))


class WizardRequestHandler(BaseHandler):
    @authenticated
    def post(self):
        from esphome import wizard

        kwargs = {k: u''.join(decode_text(x) for x in v) for k, v in self.request.arguments.items()}
        destination = settings.rel_path(kwargs['name'] + u'.yaml')
        wizard.wizard_write(path=destination, **kwargs)
        self.redirect('./?begin=True')


class DownloadBinaryRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def get(self, configuration=None):
        # pylint: disable=no-value-for-parameter
        storage_path = ext_storage_path(settings.config_dir, configuration)
        storage_json = StorageJSON.load(storage_path)
        if storage_json is None:
            self.send_error()
            return

        path = storage_json.firmware_bin_path
        self.set_header('Content-Type', 'application/octet-stream')
        filename = '{}.bin'.format(storage_json.name)
        self.set_header("Content-Disposition", 'attachment; filename="{}"'.format(filename))
        with open(path, 'rb') as f:
            while True:
                data = f.read(16384)
                if not data:
                    break
                self.write(data)
        self.finish()


def _list_dashboard_entries():
    files = settings.list_yaml_files()
    return [DashboardEntry(file) for file in files]


class DashboardEntry(object):
    def __init__(self, path):
        self.path = path
        self._storage = None
        self._loaded_storage = False

    @property
    def filename(self):
        return os.path.basename(self.path)

    @property
    def storage(self):  # type: () -> Optional[StorageJSON]
        if not self._loaded_storage:
            self._storage = StorageJSON.load(ext_storage_path(settings.config_dir, self.filename))
            self._loaded_storage = True
        return self._storage

    @property
    def address(self):
        if self.storage is None:
            return None
        return self.storage.address

    @property
    def name(self):
        if self.storage is None:
            return self.filename[:-len('.yaml')]
        return self.storage.name

    @property
    def comment(self):
        if self.storage is None:
            return None
        return self.storage.comment

    @property
    def esp_platform(self):
        if self.storage is None:
            return None
        return self.storage.esp_platform

    @property
    def board(self):
        if self.storage is None:
            return None
        return self.storage.board

    @property
    def update_available(self):
        if self.storage is None:
            return True
        return self.update_old != self.update_new

    @property
    def update_old(self):
        if self.storage is None:
            return ''
        return self.storage.esphome_version or ''

    @property
    def update_new(self):
        return const.__version__

    @property
    def loaded_integrations(self):
        if self.storage is None:
            return []
        return self.storage.loaded_integrations


class MainRequestHandler(BaseHandler):
    @authenticated
    def get(self):
        begin = bool(self.get_argument('begin', False))
        entries = _list_dashboard_entries()

        self.render("templates/index.html", entries=entries, begin=begin,
                    **template_args())


def _ping_func(filename, address):
    if os.name == 'nt':
        command = ['ping', '-n', '1', address]
    else:
        command = ['ping', '-c', '1', address]
    rc, _, _ = run_system_command(*command)
    return filename, rc == 0


class MDNSStatusThread(threading.Thread):
    def run(self):
        zc = Zeroconf()

        def on_update(dat):
            for key, b in dat.items():
                PING_RESULT[key] = b

        stat = DashboardStatus(zc, on_update)
        stat.start()
        while not STOP_EVENT.is_set():
            entries = _list_dashboard_entries()
            stat.request_query({entry.filename: entry.name + '.local.' for entry in entries})

            PING_REQUEST.wait()
            PING_REQUEST.clear()
        stat.stop()
        stat.join()
        zc.close()


class PingStatusThread(threading.Thread):
    def run(self):
        pool = multiprocessing.Pool(processes=8)
        while not STOP_EVENT.is_set():
            # Only do pings if somebody has the dashboard open

            def callback(ret):
                PING_RESULT[ret[0]] = ret[1]

            entries = _list_dashboard_entries()
            queue = collections.deque()
            for entry in entries:
                if entry.address is None:
                    PING_RESULT[entry.filename] = None
                    continue

                result = pool.apply_async(_ping_func, (entry.filename, entry.address),
                                          callback=callback)
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
        self.write(json.dumps(PING_RESULT))


def is_allowed(configuration):
    return os.path.sep not in configuration


class EditRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def get(self, configuration=None):
        filename = settings.rel_path(configuration)
        content = ''
        if os.path.isfile(filename):
            # pylint: disable=no-value-for-parameter
            with open(filename, 'r') as f:
                content = f.read()
        self.write(content)

    @authenticated
    @bind_config
    def post(self, configuration=None):
        # pylint: disable=no-value-for-parameter
        with open(settings.rel_path(configuration), 'wb') as f:
            f.write(self.request.body)
        self.set_status(200)


class DeleteRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def post(self, configuration=None):
        config_file = settings.rel_path(configuration)
        storage_path = ext_storage_path(settings.config_dir, configuration)
        storage_json = StorageJSON.load(storage_path)
        if storage_json is None:
            self.set_status(500)
            return

        name = storage_json.name
        trash_path = trash_storage_path(settings.config_dir)
        mkdir_p(trash_path)
        shutil.move(config_file, os.path.join(trash_path, configuration))

        # Delete build folder (if exists)
        build_folder = os.path.join(settings.config_dir, name)
        if build_folder is not None:
            shutil.rmtree(build_folder, os.path.join(trash_path, name))


class UndoDeleteRequestHandler(BaseHandler):
    @authenticated
    @bind_config
    def post(self, configuration=None):
        config_file = settings.rel_path(configuration)
        trash_path = trash_storage_path(settings.config_dir)
        shutil.move(os.path.join(trash_path, configuration), config_file)


PING_RESULT = {}  # type: dict
STOP_EVENT = threading.Event()
PING_REQUEST = threading.Event()


class LoginHandler(BaseHandler):
    def get(self):
        if is_authenticated(self):
            self.redirect('/')
        else:
            self.render_login_page()

    def render_login_page(self, error=None):
        self.render("templates/login.html", error=error, hassio=settings.using_hassio_auth,
                    has_username=bool(settings.username), **template_args())

    def post_hassio_login(self):
        import requests

        headers = {
            'X-HASSIO-KEY': os.getenv('HASSIO_TOKEN'),
        }
        data = {
            'username': str(self.get_argument('username', '')),
            'password': str(self.get_argument('password', ''))
        }
        try:
            req = requests.post('http://hassio/auth', headers=headers, data=data)
            if req.status_code == 200:
                self.set_secure_cookie("authenticated", cookie_authenticated_yes)
                self.redirect('/')
                return
        except Exception as err:  # pylint: disable=broad-except
            _LOGGER.warning("Error during Hass.io auth request: %s", err)
            self.set_status(500)
            self.render_login_page(error="Internal server error")
            return
        self.set_status(401)
        self.render_login_page(error="Invalid username or password")

    def post_native_login(self):
        username = str(self.get_argument("username", '').encode('utf-8'))
        password = str(self.get_argument("password", '').encode('utf-8'))
        if settings.check_password(username, password):
            self.set_secure_cookie("authenticated", cookie_authenticated_yes)
            self.redirect("/")
            return
        error_str = "Invalid username or password" if settings.username else "Invalid password"
        self.set_status(401)
        self.render_login_page(error=error_str)

    def post(self):
        if settings.using_hassio_auth:
            self.post_hassio_login()
        else:
            self.post_native_login()


class LogoutHandler(BaseHandler):
    @authenticated
    def get(self):
        self.clear_cookie("authenticated")
        self.redirect('./login')


_STATIC_FILE_HASHES = {}


def get_static_file_url(name):
    static_path = os.path.join(os.path.dirname(__file__), 'static')
    if name in _STATIC_FILE_HASHES:
        hash_ = _STATIC_FILE_HASHES[name]
    else:
        path = os.path.join(static_path, name)
        with open(path, 'rb') as f_handle:
            hash_ = hashlib.md5(f_handle.read()).hexdigest()[:8]
        _STATIC_FILE_HASHES[name] = hash_
    return u'./static/{}?hash={}'.format(name, hash_)


def make_app(debug=False):
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
        log_method("%d %s %.2fms", handler.get_status(),
                   handler._request_summary(), request_time)

    class StaticFileHandler(tornado.web.StaticFileHandler):
        def set_extra_headers(self, path):
            if debug:
                self.set_header('Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0')

    static_path = os.path.join(os.path.dirname(__file__), 'static')
    app_settings = {
        'debug': debug,
        'cookie_secret': settings.cookie_secret,
        'log_function': log_function,
        'websocket_ping_interval': 30.0,
    }
    rel = settings.relative_url
    app = tornado.web.Application([
        (rel + "", MainRequestHandler),
        (rel + "login", LoginHandler),
        (rel + "logout", LogoutHandler),
        (rel + "logs", EsphomeLogsHandler),
        (rel + "upload", EsphomeUploadHandler),
        (rel + "compile", EsphomeCompileHandler),
        (rel + "validate", EsphomeValidateHandler),
        (rel + "clean-mqtt", EsphomeCleanMqttHandler),
        (rel + "clean", EsphomeCleanHandler),
        (rel + "vscode", EsphomeVscodeHandler),
        (rel + "ace", EsphomeAceEditorHandler),
        (rel + "update-all", EsphomeUpdateAllHandler),
        (rel + "edit", EditRequestHandler),
        (rel + "download.bin", DownloadBinaryRequestHandler),
        (rel + "serial-ports", SerialPortRequestHandler),
        (rel + "ping", PingRequestHandler),
        (rel + "delete", DeleteRequestHandler),
        (rel + "undo-delete", UndoDeleteRequestHandler),
        (rel + "wizard.html", WizardRequestHandler),
        (rel + r"static/(.*)", StaticFileHandler, {'path': static_path}),
    ], **app_settings)

    if debug:
        _STATIC_FILE_HASHES.clear()

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
        _LOGGER.info("Starting dashboard web server on unix socket %s and configuration dir %s...",
                     args.socket, settings.config_dir)
        server = tornado.httpserver.HTTPServer(app)
        socket = tornado.netutil.bind_unix_socket(args.socket, mode=0o666)
        server.add_socket(socket)
    else:
        _LOGGER.info("Starting dashboard web server on port %s and configuration dir %s...",
                     args.port, settings.config_dir)
        app.listen(args.port)

        if args.open_ui:
            import webbrowser

            webbrowser.open('localhost:{}'.format(args.port))

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
