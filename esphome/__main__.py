from __future__ import print_function

import argparse
from collections import OrderedDict
from datetime import datetime
import logging
import os
import random
import sys

from esphome import const, core_config, mqtt, platformio_api, wizard, writer, yaml_util
from esphome.api.client import run_logs
from esphome.config import get_component, iter_components, read_config, strip_default_ids
from esphome.const import CONF_BAUD_RATE, CONF_BROKER, CONF_ESPHOME, CONF_LOGGER, \
    CONF_USE_CUSTOM_CODE
from esphome.core import CORE, EsphomeError
from esphome.cpp_generator import Expression, RawStatement, add, statement
from esphome.helpers import color, indent
from esphome.py_compat import IS_PY2, safe_input, text_type
from esphome.storage_json import StorageJSON, esphome_storage_path, \
    start_update_check_thread, storage_path
from esphome.util import run_external_command, run_external_process, safe_print

_LOGGER = logging.getLogger(__name__)

PRE_INITIALIZE = ['esphome', 'logger', 'wifi', 'ethernet', 'ota', 'mqtt', 'web_server', 'api',
                  'i2c']


def get_serial_ports():
    # from https://github.com/pyserial/pyserial/blob/master/serial/tools/list_ports.py
    from serial.tools.list_ports import comports
    result = []
    for port, desc, info in comports():
        if not port:
            continue
        if "VID:PID" in info:
            result.append((port, desc))
    result.sort(key=lambda x: x[0])
    return result


def choose_prompt(options):
    if not options:
        raise ValueError

    if len(options) == 1:
        return options[0][1]

    safe_print(u"Found multiple options, please choose one:")
    for i, (desc, _) in enumerate(options):
        safe_print(u"  [{}] {}".format(i + 1, desc))

    while True:
        opt = safe_input('(number): ')
        if opt in options:
            opt = options.index(opt)
            break
        try:
            opt = int(opt)
            if opt < 1 or opt > len(options):
                raise ValueError
            break
        except ValueError:
            safe_print(color('red', u"Invalid option: '{}'".format(opt)))
    return options[opt - 1][1]


def choose_upload_log_host(default, check_default, show_ota, show_mqtt, show_api):
    options = []
    for res, desc in get_serial_ports():
        options.append((u"{} ({})".format(res, desc), res))
    if (show_ota and 'ota' in CORE.config) or (show_api and 'api' in CORE.config):
        options.append((u"Over The Air ({})".format(CORE.address), CORE.address))
        if default == 'OTA':
            return CORE.address
    if show_mqtt and 'mqtt' in CORE.config:
        options.append((u"MQTT ({})".format(CORE.config['mqtt'][CONF_BROKER]), 'MQTT'))
        if default == 'OTA':
            return 'MQTT'
    if default is not None:
        return default
    if check_default is not None and check_default in [opt[1] for opt in options]:
        return check_default
    return choose_prompt(options)


def get_port_type(port):
    if port.startswith('/') or port.startswith('COM'):
        return 'SERIAL'
    if port == 'MQTT':
        return 'MQTT'
    return 'NETWORK'


def run_miniterm(config, port):
    import serial
    if CONF_LOGGER not in config:
        _LOGGER.info("Logger is not enabled. Not starting UART logs.")
        return
    baud_rate = config['logger'][CONF_BAUD_RATE]
    if baud_rate == 0:
        _LOGGER.info("UART logging is disabled (baud_rate=0). Not starting UART logs.")
    _LOGGER.info("Starting log output from %s with baud rate %s", port, baud_rate)

    backtrace_state = False
    with serial.Serial(port, baudrate=baud_rate) as ser:
        while True:
            try:
                raw = ser.readline()
            except serial.SerialException:
                _LOGGER.error("Serial port closed!")
                return
            if IS_PY2:
                line = raw.replace('\r', '').replace('\n', '')
            else:
                line = raw.replace(b'\r', b'').replace(b'\n', b'').decode('utf8',
                                                                          'backslashreplace')
            time = datetime.now().time().strftime('[%H:%M:%S]')
            message = time + line
            safe_print(message)

            backtrace_state = platformio_api.process_stacktrace(
                config, line, backtrace_state=backtrace_state)


def write_cpp(config):
    _LOGGER.info("Generating C++ source...")

    CORE.add_job(core_config.to_code, config[CONF_ESPHOME], domain='esphome')
    for domain in PRE_INITIALIZE:
        if domain == CONF_ESPHOME or domain not in config:
            continue
        CORE.add_job(get_component(domain).to_code, config[domain], domain=domain)

    for domain, component, conf in iter_components(config):
        if domain in PRE_INITIALIZE or not hasattr(component, 'to_code'):
            continue
        CORE.add_job(component.to_code, conf, domain=domain)

    CORE.flush_tasks()
    add(RawStatement(''))
    add(RawStatement(''))
    all_code = []
    for exp in CORE.expressions:
        if not config[CONF_ESPHOME][CONF_USE_CUSTOM_CODE]:
            if isinstance(exp, Expression) and not exp.required:
                continue
        all_code.append(text_type(statement(exp)))

    writer.write_platformio_project()

    code_s = indent('\n'.join(line.rstrip() for line in all_code))
    writer.write_cpp(code_s)
    return 0


def compile_program(args, config):
    _LOGGER.info("Compiling app...")
    update_check = not os.getenv('ESPHOME_NO_UPDATE_CHECK', '')
    if update_check:
        thread = start_update_check_thread(esphome_storage_path(CORE.config_dir))
    rc = platformio_api.run_compile(config, args.verbose)
    if update_check:
        thread.join()
    return rc


def upload_using_esptool(config, port):
    path = os.path.join(CORE.build_path, '.pioenvs', CORE.name, 'firmware.bin')
    cmd = ['esptool.py', '--before', 'default_reset', '--after', 'hard_reset',
           '--chip', 'esp8266', '--port', port, 'write_flash', '0x0', path]

    if os.environ.get('ESPHOME_USE_SUBPROCESS') is None:
        import esptool
        # pylint: disable=protected-access
        return run_external_command(esptool._main, *cmd)

    return run_external_process(*cmd)


def upload_program(config, args, host):
    # if upload is to a serial port use platformio, otherwise assume ota
    if get_port_type(host) == 'SERIAL':
        if CORE.is_esp8266:
            return upload_using_esptool(config, host)
        return platformio_api.run_upload(config, args.verbose, host)

    from esphome.components import ota
    from esphome import espota2

    if args.host_port is not None:
        host_port = args.host_port
    else:
        host_port = int(os.getenv('ESPHOME_OTA_HOST_PORT', random.randint(10000, 60000)))

    verbose = args.verbose
    remote_port = ota.get_port(config)
    password = ota.get_auth(config)

    storage = StorageJSON.load(storage_path())
    res = espota2.run_ota(host, remote_port, password, CORE.firmware_bin)
    if res == 0:
        if storage is not None and storage.use_legacy_ota:
            storage.use_legacy_ota = False
            storage.save(storage_path())
        return res
    if storage is not None and not storage.use_legacy_ota:
        return res

    _LOGGER.warning("OTA v2 method failed. Trying with legacy OTA...")
    return espota2.run_legacy_ota(verbose, host_port, host, remote_port, password,
                                  CORE.firmware_bin)


def show_logs(config, args, port):
    if 'logger' not in config:
        raise EsphomeError("Logger is not configured!")
    if get_port_type(port) == 'SERIAL':
        run_miniterm(config, port)
        return 0
    if get_port_type(port) == 'NETWORK' and 'api' in config:
        return run_logs(config, port)
    if get_port_type(port) == 'MQTT' and 'mqtt' in config:
        return mqtt.show_logs(config, args.topic, args.username, args.password, args.client_id)

    raise ValueError


def clean_mqtt(config, args):
    return mqtt.clear_topic(config, args.topic, args.username, args.password, args.client_id)


def setup_log(debug=False):
    log_level = logging.DEBUG if debug else logging.INFO
    logging.basicConfig(level=log_level)
    fmt = "%(levelname)s %(message)s"
    colorfmt = "%(log_color)s{}%(reset)s".format(fmt)
    datefmt = '%H:%M:%S'

    logging.getLogger('urllib3').setLevel(logging.WARNING)

    try:
        from colorlog import ColoredFormatter
        logging.getLogger().handlers[0].setFormatter(ColoredFormatter(
            colorfmt,
            datefmt=datefmt,
            reset=True,
            log_colors={
                'DEBUG': 'cyan',
                'INFO': 'green',
                'WARNING': 'yellow',
                'ERROR': 'red',
                'CRITICAL': 'red',
            }
        ))
    except ImportError:
        pass


def command_wizard(args):
    return wizard.wizard(args.configuration)


def command_config(args, config):
    _LOGGER.info("Configuration is valid!")
    if not args.verbose:
        config = strip_default_ids(config)
    safe_print(yaml_util.dump(config))
    return 0


def command_compile(args, config):
    exit_code = write_cpp(config)
    if exit_code != 0:
        return exit_code
    if args.only_generate:
        _LOGGER.info(u"Successfully generated source code.")
        return 0
    exit_code = compile_program(args, config)
    if exit_code != 0:
        return exit_code
    _LOGGER.info(u"Successfully compiled program.")
    return 0


def command_upload(args, config):
    port = choose_upload_log_host(default=args.upload_port, check_default=None,
                                  show_ota=True, show_mqtt=False, show_api=False)
    exit_code = upload_program(config, args, port)
    if exit_code != 0:
        return exit_code
    _LOGGER.info(u"Successfully uploaded program.")
    return 0


def command_logs(args, config):
    port = choose_upload_log_host(default=args.serial_port, check_default=None,
                                  show_ota=False, show_mqtt=True, show_api=True)
    return show_logs(config, args, port)


def command_run(args, config):
    exit_code = write_cpp(config)
    if exit_code != 0:
        return exit_code
    exit_code = compile_program(args, config)
    if exit_code != 0:
        return exit_code
    _LOGGER.info(u"Successfully compiled program.")
    port = choose_upload_log_host(default=args.upload_port, check_default=None,
                                  show_ota=True, show_mqtt=False, show_api=True)
    exit_code = upload_program(config, args, port)
    if exit_code != 0:
        return exit_code
    _LOGGER.info(u"Successfully uploaded program.")
    if args.no_logs:
        return 0
    port = choose_upload_log_host(default=args.upload_port, check_default=port,
                                  show_ota=False, show_mqtt=True, show_api=True)
    return show_logs(config, args, port)


def command_clean_mqtt(args, config):
    return clean_mqtt(config, args)


def command_mqtt_fingerprint(args, config):
    return mqtt.get_fingerprint(config)


def command_version(args):
    safe_print(u"Version: {}".format(const.__version__))
    return 0


def command_clean(args, config):
    try:
        writer.clean_build()
    except OSError as err:
        _LOGGER.error("Error deleting build files: %s", err)
        return 1
    _LOGGER.info("Done!")
    return 0


def command_hass_config(args, config):
    from esphome.components import mqtt as mqtt_component

    _LOGGER.info("This is what you should put in your Home Assistant YAML configuration.")
    _LOGGER.info("Please note this is only necessary if you're not using MQTT discovery.")
    data = mqtt_component.GenerateHassConfigData(config)
    hass_config = OrderedDict()
    for domain, component, conf in iter_components(config):
        if not hasattr(component, 'to_hass_config'):
            continue
        func = getattr(component, 'to_hass_config')
        ret = func(data, conf)
        if not isinstance(ret, (list, tuple)):
            ret = [ret]
        ret = [x for x in ret if x is not None]
        domain_conf = hass_config.setdefault(domain.split('.')[0], [])
        domain_conf += ret

    safe_print(yaml_util.dump(hass_config))
    return 0


def command_dashboard(args):
    from esphome.dashboard import dashboard

    return dashboard.start_web_server(args)


PRE_CONFIG_ACTIONS = {
    'wizard': command_wizard,
    'version': command_version,
    'dashboard': command_dashboard
}

POST_CONFIG_ACTIONS = {
    'config': command_config,
    'compile': command_compile,
    'upload': command_upload,
    'logs': command_logs,
    'run': command_run,
    'clean-mqtt': command_clean_mqtt,
    'mqtt-fingerprint': command_mqtt_fingerprint,
    'clean': command_clean,
    'hass-config': command_hass_config,
}


def parse_args(argv):
    parser = argparse.ArgumentParser(prog='esphome')
    parser.add_argument('-v', '--verbose', help="Enable verbose esphome logs.",
                        action='store_true')
    parser.add_argument('--dashboard', help="Internal flag to set if the command is run from the "
                                            "dashboard.", action='store_true')
    parser.add_argument('configuration', help='Your YAML configuration file.')

    subparsers = parser.add_subparsers(help='Commands', dest='command')
    subparsers.required = True
    subparsers.add_parser('config', help='Validate the configuration and spit it out.')

    parser_compile = subparsers.add_parser('compile',
                                           help='Read the configuration and compile a program.')
    parser_compile.add_argument('--only-generate',
                                help="Only generate source code, do not compile.",
                                action='store_true')

    parser_upload = subparsers.add_parser('upload', help='Validate the configuration '
                                                         'and upload the latest binary.')
    parser_upload.add_argument('--upload-port', help="Manually specify the upload port to use. "
                                                     "For example /dev/cu.SLAB_USBtoUART.")
    parser_upload.add_argument('--host-port', help="Specify the host port.", type=int)

    parser_logs = subparsers.add_parser('logs', help='Validate the configuration '
                                                     'and show all MQTT logs.')
    parser_logs.add_argument('--topic', help='Manually set the topic to subscribe to.')
    parser_logs.add_argument('--username', help='Manually set the username.')
    parser_logs.add_argument('--password', help='Manually set the password.')
    parser_logs.add_argument('--client-id', help='Manually set the client id.')
    parser_logs.add_argument('--serial-port', help="Manually specify a serial port to use"
                                                   "For example /dev/cu.SLAB_USBtoUART.")

    parser_run = subparsers.add_parser('run', help='Validate the configuration, create a binary, '
                                                   'upload it, and start MQTT logs.')
    parser_run.add_argument('--upload-port', help="Manually specify the upload port/ip to use. "
                                                  "For example /dev/cu.SLAB_USBtoUART.")
    parser_run.add_argument('--host-port', help="Specify the host port to use for OTA", type=int)
    parser_run.add_argument('--no-logs', help='Disable starting MQTT logs.',
                            action='store_true')
    parser_run.add_argument('--topic', help='Manually set the topic to subscribe to for logs.')
    parser_run.add_argument('--username', help='Manually set the MQTT username for logs.')
    parser_run.add_argument('--password', help='Manually set the MQTT password for logs.')
    parser_run.add_argument('--client-id', help='Manually set the client id for logs.')

    parser_clean = subparsers.add_parser('clean-mqtt', help="Helper to clear an MQTT topic from "
                                                            "retain messages.")
    parser_clean.add_argument('--topic', help='Manually set the topic to subscribe to.')
    parser_clean.add_argument('--username', help='Manually set the username.')
    parser_clean.add_argument('--password', help='Manually set the password.')
    parser_clean.add_argument('--client-id', help='Manually set the client id.')

    subparsers.add_parser('wizard', help="A helpful setup wizard that will guide "
                                         "you through setting up esphome.")

    subparsers.add_parser('mqtt-fingerprint', help="Get the SSL fingerprint from a MQTT broker.")

    subparsers.add_parser('version', help="Print the esphome version and exit.")

    subparsers.add_parser('clean', help="Delete all temporary build files.")

    dashboard = subparsers.add_parser('dashboard',
                                      help="Create a simple web server for a dashboard.")
    dashboard.add_argument("--port", help="The HTTP port to open connections on. Defaults to 6052.",
                           type=int, default=6052)
    dashboard.add_argument("--password", help="The optional password to require for all requests.",
                           type=str, default='')
    dashboard.add_argument("--open-ui", help="Open the dashboard UI in a browser.",
                           action='store_true')
    dashboard.add_argument("--hassio",
                           help="Internal flag used to tell esphome is started as a Hass.io "
                                "add-on.",
                           action="store_true")
    dashboard.add_argument("--socket",
                           help="Make the dashboard serve under a unix socket", type=str)

    subparsers.add_parser('hass-config',
                          help="Dump the configuration entries that should be added "
                               "to Home Assistant when not using MQTT discovery.")

    return parser.parse_args(argv[1:])


def run_esphome(argv):
    args = parse_args(argv)
    CORE.dashboard = args.dashboard

    setup_log(args.verbose)
    if args.command in PRE_CONFIG_ACTIONS:
        try:
            return PRE_CONFIG_ACTIONS[args.command](args)
        except EsphomeError as e:
            _LOGGER.error(e)
            return 1

    CORE.config_path = args.configuration

    config = read_config(args.verbose)
    if config is None:
        return 1
    CORE.config = config

    if args.command in POST_CONFIG_ACTIONS:
        try:
            return POST_CONFIG_ACTIONS[args.command](args, config)
        except EsphomeError as e:
            _LOGGER.error(e)
            return 1
    safe_print(u"Unknown command {}".format(args.command))
    return 1


def main():
    try:
        return run_esphome(sys.argv)
    except EsphomeError as e:
        _LOGGER.error(e)
        return 1
    except KeyboardInterrupt:
        return 1


if __name__ == "__main__":
    sys.exit(main())
