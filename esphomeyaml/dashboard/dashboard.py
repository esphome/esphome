from __future__ import print_function

import codecs
import json
import logging
import os
import random
import subprocess

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
    pass

from esphomeyaml import const, core, __main__
from esphomeyaml.__main__ import get_serial_ports, get_base_path, get_name
from esphomeyaml.helpers import quote

_LOGGER = logging.getLogger(__name__)
CONFIG_DIR = ''

# pylint: disable=abstract-method, arguments-differ
class EsphomeyamlCommandWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self, application, request, **kwargs):
        super(EsphomeyamlCommandWebSocket, self).__init__(application, request, **kwargs)
        self.proc = None
        self.closed = False

    def on_message(self, message):
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
            data = data.replace('\033', '\\033')
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


class SerialPortRequestHandler(tornado.web.RequestHandler):
    def get(self):
        ports = get_serial_ports()
        data = []
        for port, desc in ports:
            if port == '/dev/ttyAMA0':
                # ignore RPi built-in serial port
                continue
            data.append({'port': port, 'desc': desc})
        data.append({'port': 'OTA', 'desc': 'Over-The-Air Upload/Logs'})
        self.write(json.dumps(data))


class WizardRequestHandler(tornado.web.RequestHandler):
    def post(self):
        from esphomeyaml import wizard

        kwargs = {k: ''.join(v) for k, v in self.request.arguments.iteritems()}
        config = wizard.wizard_file(**kwargs)
        destination = os.path.join(CONFIG_DIR, kwargs['name'] + '.yaml')
        with codecs.open(destination, 'w') as f_handle:
            f_handle.write(config)

        self.redirect('/')


class DownloadBinaryRequestHandler(tornado.web.RequestHandler):
    def get(self):
        configuration = self.get_argument('configuration')
        config_file = os.path.join(CONFIG_DIR, configuration)
        core.CONFIG_PATH = config_file
        config = __main__.read_config(core.CONFIG_PATH)
        name = get_name(config)
        path = os.path.join(get_base_path(config), '.pioenvs', name, 'firmware.bin')
        self.set_header('Content-Type', 'application/octet-stream')
        self.set_header("Content-Disposition", 'attachment; filename="{}.bin"'.format(name))
        with open(path, 'rb') as f:
            while 1:
                data = f.read(16384)  # or some other nice-sized chunk
                if not data:
                    break
                self.write(data)
        self.finish()


class MainRequestHandler(tornado.web.RequestHandler):
    def get(self):
        files = sorted([f for f in os.listdir(CONFIG_DIR) if f.endswith('.yaml') and
                        not f.startswith('.')])
        full_path_files = [os.path.join(CONFIG_DIR, f) for f in files]
        self.render("templates/index.html", files=files, full_path_files=full_path_files,
                    version=const.__version__)


def make_app():
    static_path = os.path.join(os.path.dirname(__file__), 'static')
    return tornado.web.Application([
        (r"/", MainRequestHandler),
        (r"/logs", EsphomeyamlLogsHandler),
        (r"/run", EsphomeyamlRunHandler),
        (r"/compile", EsphomeyamlCompileHandler),
        (r"/download.bin", DownloadBinaryRequestHandler),
        (r"/serial-ports", SerialPortRequestHandler),
        (r"/wizard.html", WizardRequestHandler),
        (r'/static/(.*)', tornado.web.StaticFileHandler, {'path': static_path}),
    ], debug=False)


def start_web_server(args):
    global CONFIG_DIR
    CONFIG_DIR = args.configuration
    if not os.path.exists(CONFIG_DIR):
        os.makedirs(CONFIG_DIR)

    _LOGGER.info("Starting dashboard web server on port %s and configuration dir %s...",
                 args.port, CONFIG_DIR)
    app = make_app()
    app.listen(args.port)
    try:
        tornado.ioloop.IOLoop.current().start()
    except KeyboardInterrupt:
        _LOGGER.info("Shutting down...")
