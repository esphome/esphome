# pylint: disable=wrong-import-position
from __future__ import print_function

import codecs
import hmac
import json
import logging
import os
import random
import subprocess

from esphomeyaml.const import CONF_ESPHOMEYAML, CONF_BUILD_PATH
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml import const, core, __main__
from esphomeyaml.__main__ import get_serial_ports
from esphomeyaml.helpers import quote, relative_path

try:
    import tornado
    import tornado.gen
    import tornado.ioloop
    import tornado.iostream
    import tornado.process
    import tornado.web
    import tornado.websocket
    import tornado.concurrent
except ImportError as err:
    tornado = None

_LOGGER = logging.getLogger(__name__)
CONFIG_DIR = ''
PASSWORD = ''


# pylint: disable=abstract-method
class BaseHandler(tornado.web.RequestHandler):
    def is_authenticated(self):
        return not PASSWORD or self.get_secure_cookie('authenticated') == 'yes'


# pylint: disable=abstract-method, arguments-differ
class EsphomeyamlCommandWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self, application, request, **kwargs):
        super(EsphomeyamlCommandWebSocket, self).__init__(application, request, **kwargs)
        self.proc = None
        self.closed = False

    def on_message(self, message):
        if PASSWORD and self.get_secure_cookie('authenticated') != 'yes':
            return
        if self.proc is not None:
            return
        command = self.build_command(message)
        _LOGGER.debug(u"WebSocket opened for command %s", [quote(x) for x in command])
        self.proc = tornado.process.Subprocess(command,
                                               stdout=tornado.process.Subprocess.STREAM,
                                               stderr=subprocess.STDOUT)
        self.proc.set_exit_callback(self.proc_on_exit)
        tornado.ioloop.IOLoop.current().spawn_callback(self.redirect_stream)

    @tornado.gen.coroutine
    def redirect_stream(self):
        while True:
            try:
                data = yield self.proc.stdout.read_until_regex('[\n\r]')
            except tornado.iostream.StreamClosedError:
                break
            if data.endswith('\r') and random.randrange(100) < 90:
                continue
            try:
                data = data.replace('\033', '\\033')
            except UnicodeDecodeError:
                data = data.encode('ascii', 'backslashreplace')
            self.write_message({'event': 'line', 'data': data})

    def proc_on_exit(self, returncode):
        if not self.closed:
            _LOGGER.debug("Process exited with return code %s", returncode)
            self.write_message({'event': 'exit', 'code': returncode})

    def on_close(self):
        self.closed = True
        if self.proc is not None and self.proc.returncode is None:
            _LOGGER.debug("Terminating process")
            self.proc.proc.terminate()

    def build_command(self, message):
        raise NotImplementedError


class EsphomeyamlLogsHandler(EsphomeyamlCommandWebSocket):
    def build_command(self, message):
        js = json.loads(message)
        config_file = CONFIG_DIR + '/' + js['configuration']
        return ["esphomeyaml", config_file, "logs", '--serial-port', js["port"], '--escape']


class EsphomeyamlRunHandler(EsphomeyamlCommandWebSocket):
    def build_command(self, message):
        js = json.loads(message)
        config_file = os.path.join(CONFIG_DIR, js['configuration'])
        return ["esphomeyaml", config_file, "run", '--upload-port', js["port"],
                '--escape', '--use-esptoolpy']


class EsphomeyamlCompileHandler(EsphomeyamlCommandWebSocket):
    def build_command(self, message):
        js = json.loads(message)
        config_file = os.path.join(CONFIG_DIR, js['configuration'])
        return ["esphomeyaml", config_file, "compile"]


class EsphomeyamlValidateHandler(EsphomeyamlCommandWebSocket):
    def build_command(self, message):
        js = json.loads(message)
        config_file = os.path.join(CONFIG_DIR, js['configuration'])
        return ["esphomeyaml", config_file, "config"]


class EsphomeyamlCleanMqttHandler(EsphomeyamlCommandWebSocket):
    def build_command(self, message):
        js = json.loads(message)
        config_file = os.path.join(CONFIG_DIR, js['configuration'])
        return ["esphomeyaml", config_file, "clean-mqtt"]


class EsphomeyamlCleanHandler(EsphomeyamlCommandWebSocket):
    def build_command(self, message):
        js = json.loads(message)
        config_file = os.path.join(CONFIG_DIR, js['configuration'])
        return ["esphomeyaml", config_file, "clean"]


class SerialPortRequestHandler(BaseHandler):
    def get(self):
        if not self.is_authenticated():
            self.redirect('/login')
            return
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
        self.write(json.dumps(sorted(data, reverse=True)))


class WizardRequestHandler(BaseHandler):
    def post(self):
        from esphomeyaml import wizard

        if not self.is_authenticated():
            self.redirect('/login')
            return
        kwargs = {k: ''.join(v) for k, v in self.request.arguments.iteritems()}
        config = wizard.wizard_file(**kwargs)
        destination = os.path.join(CONFIG_DIR, kwargs['name'] + '.yaml')
        with codecs.open(destination, 'w') as f_handle:
            f_handle.write(config)

        self.redirect('/?begin=True')


class DownloadBinaryRequestHandler(BaseHandler):
    def get(self):
        if not self.is_authenticated():
            self.redirect('/login')
            return

        configuration = self.get_argument('configuration')
        config_file = os.path.join(CONFIG_DIR, configuration)
        core.CONFIG_PATH = config_file
        config = __main__.read_config(core.CONFIG_PATH)
        build_path = relative_path(config[CONF_ESPHOMEYAML][CONF_BUILD_PATH])
        path = os.path.join(build_path, '.pioenvs', core.NAME, 'firmware.bin')
        self.set_header('Content-Type', 'application/octet-stream')
        self.set_header("Content-Disposition", 'attachment; filename="{}.bin"'.format(core.NAME))
        with open(path, 'rb') as f:
            while 1:
                data = f.read(16384)  # or some other nice-sized chunk
                if not data:
                    break
                self.write(data)
        self.finish()


class MainRequestHandler(BaseHandler):
    def get(self):
        if not self.is_authenticated():
            self.redirect('/login')
            return

        begin = bool(self.get_argument('begin', False))
        files = sorted([f for f in os.listdir(CONFIG_DIR) if f.endswith('.yaml') and
                        not f.startswith('.')])
        full_path_files = [os.path.join(CONFIG_DIR, f) for f in files]
        self.render("templates/index.html", files=files, full_path_files=full_path_files,
                    version=const.__version__, begin=begin)


class LoginHandler(BaseHandler):
    def get(self):
        self.write('<html><body><form action="/login" method="post">'
                   'Password: <input type="password" name="password">'
                   '<input type="submit" value="Sign in">'
                   '</form></body></html>')

    def post(self):
        password = str(self.get_argument("password", ''))
        password = hmac.new(password).digest()
        if hmac.compare_digest(PASSWORD, password):
            self.set_secure_cookie("authenticated", "yes")
        self.redirect("/")


def make_app(debug=False):
    static_path = os.path.join(os.path.dirname(__file__), 'static')
    return tornado.web.Application([
        (r"/", MainRequestHandler),
        (r"/login", LoginHandler),
        (r"/logs", EsphomeyamlLogsHandler),
        (r"/run", EsphomeyamlRunHandler),
        (r"/compile", EsphomeyamlCompileHandler),
        (r"/validate", EsphomeyamlValidateHandler),
        (r"/clean-mqtt", EsphomeyamlCleanMqttHandler),
        (r"/clean", EsphomeyamlCleanHandler),
        (r"/download.bin", DownloadBinaryRequestHandler),
        (r"/serial-ports", SerialPortRequestHandler),
        (r"/wizard.html", WizardRequestHandler),
        (r'/static/(.*)', tornado.web.StaticFileHandler, {'path': static_path}),
    ], debug=debug, cookie_secret=PASSWORD)


def start_web_server(args):
    global CONFIG_DIR
    global PASSWORD

    if tornado is None:
        raise ESPHomeYAMLError("Attempted to load dashboard, but tornado is not installed! "
                               "Please run \"pip2 install tornado esptool\" in your terminal.")

    CONFIG_DIR = args.configuration
    if not os.path.exists(CONFIG_DIR):
        os.makedirs(CONFIG_DIR)

    # HassIO options storage
    PASSWORD = args.password
    if os.path.isfile('/data/options.json'):
        with open('/data/options.json') as f:
            js = json.load(f)
            PASSWORD = js.get('password') or PASSWORD

    if PASSWORD:
        PASSWORD = hmac.new(str(PASSWORD)).digest()
        # Use the digest of the password as our cookie secret. This makes sure the cookie
        # isn't too short. It, of course, enables local hash brute forcing (because the cookie
        # secret can be brute forced without making requests). But the hashing algorithm used
        # by tornado is apparently strong enough to make brute forcing even a short string pretty
        # hard.

    _LOGGER.info("Starting dashboard web server on port %s and configuration dir %s...",
                 args.port, CONFIG_DIR)
    app = make_app(args.verbose)
    app.listen(args.port)

    if args.open_ui:
        import webbrowser

        webbrowser.open('localhost:{}'.format(args.port))

    try:
        tornado.ioloop.IOLoop.current().start()
    except KeyboardInterrupt:
        _LOGGER.info("Shutting down...")
