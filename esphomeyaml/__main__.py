from __future__ import print_function

import argparse
import logging
import os
import random
import sys
from datetime import datetime

from esphomeyaml import const, core, core_config, mqtt, wizard, writer, yaml_util
from esphomeyaml.config import get_component, iter_components, read_config
from esphomeyaml.const import CONF_BAUD_RATE, CONF_BUILD_PATH, CONF_DOMAIN, CONF_ESPHOMEYAML, \
    CONF_HOSTNAME, CONF_LOGGER, CONF_MANUAL_IP, CONF_NAME, CONF_STATIC_IP, CONF_USE_CUSTOM_CODE, \
    CONF_WIFI, ESP_PLATFORM_ESP8266
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import AssignmentExpression, Expression, RawStatement, \
    _EXPRESSIONS, add, \
    add_job, color, flush_tasks, indent, quote, statement, relative_path
from esphomeyaml.util import safe_print

_LOGGER = logging.getLogger(__name__)

PRE_INITIALIZE = ['esphomeyaml', 'logger', 'wifi', 'ota', 'mqtt', 'web_server', 'i2c']


def get_serial_ports():
    # from https://github.com/pyserial/pyserial/blob/master/serial/tools/list_ports.py
    from serial.tools.list_ports import comports
    result = []
    for port, desc, info in comports():
        if not port:
            continue
        if "VID:PID" in info:
            result.append((port, desc))
    return result


def choose_serial_port(config):
    result = get_serial_ports()

    if not result:
        return 'OTA'
    safe_print(u"Found multiple serial port options, please choose one:")
    for i, (res, desc) in enumerate(result):
        safe_print(u"  [{}] {} ({})".format(i, res, desc))
    safe_print(u"  [{}] Over The Air ({})".format(len(result), get_upload_host(config)))
    safe_print()
    while True:
        opt = raw_input('(number): ')
        if opt in result:
            opt = result.index(opt)
            break
        try:
            opt = int(opt)
            if opt < 0 or opt > len(result):
                raise ValueError
            break
        except ValueError:
            safe_print(color('red', u"Invalid option: '{}'".format(opt)))
    if opt == len(result):
        return 'OTA'
    return result[opt][0]


def run_platformio(*cmd, **kwargs):
    def mock_exit(return_code):
        raise SystemExit(return_code)

    orig_argv = sys.argv
    orig_exit = sys.exit  # mock sys.exit
    full_cmd = u' '.join(quote(x) for x in cmd)
    _LOGGER.info(u"Running:  %s", full_cmd)
    try:
        func = kwargs.get('main')
        if func is None:
            import platformio.__main__
            func = platformio.__main__.main
        sys.argv = list(cmd)
        sys.exit = mock_exit
        return func() or 0
    except KeyboardInterrupt:
        return 1
    except SystemExit as err:
        return err.args[0]
    except Exception as err:  # pylint: disable=broad-except
        _LOGGER.error(u"Running platformio failed: %s", err)
        _LOGGER.error(u"Please try running %s locally.", full_cmd)
    finally:
        sys.argv = orig_argv
        sys.exit = orig_exit


def run_miniterm(config, port, escape=False):
    import serial
    if CONF_LOGGER not in config:
        _LOGGER.info("Logger is not enabled. Not starting UART logs.")
        return
    baud_rate = config['logger'][CONF_BAUD_RATE]
    if baud_rate == 0:
        _LOGGER.info("UART logging is disabled (baud_rate=0). Not starting UART logs.")
    _LOGGER.info("Starting log output from %s with baud rate %s", port, baud_rate)

    with serial.Serial(port, baudrate=baud_rate) as ser:
        while True:
            try:
                raw = ser.readline()
            except serial.SerialException:
                _LOGGER.error("Serial port closed!")
                return
            line = raw.replace('\r', '').replace('\n', '')
            time = datetime.now().time().strftime('[%H:%M:%S]')
            message = time + line
            if escape:
                message = message.replace('\033', '\\033')
            safe_print(message)


def write_cpp(config):
    _LOGGER.info("Generating C++ source...")

    add_job(core_config.to_code, config[CONF_ESPHOMEYAML], domain='esphomeyaml')
    for domain in PRE_INITIALIZE:
        if domain == CONF_ESPHOMEYAML or domain not in config:
            continue
        add_job(get_component(domain).to_code, config[domain], domain=domain)

    for domain, component, conf in iter_components(config):
        if domain in PRE_INITIALIZE or not hasattr(component, 'to_code'):
            continue
        add_job(component.to_code, conf, domain=domain)

    flush_tasks()
    add(RawStatement(''))
    add(RawStatement(''))
    all_code = []
    for exp in _EXPRESSIONS:
        if not config[CONF_ESPHOMEYAML][CONF_USE_CUSTOM_CODE]:
            if isinstance(exp, Expression) and not exp.required:
                continue
            if isinstance(exp, AssignmentExpression) and not exp.obj.required:
                if not exp.has_side_effects():
                    continue
                exp = exp.rhs
        all_code.append(unicode(statement(exp)))

    build_path = relative_path(config[CONF_ESPHOMEYAML][CONF_BUILD_PATH])
    writer.write_platformio_project(config, build_path)

    code_s = indent('\n'.join(line.rstrip() for line in all_code))
    cpp_path = os.path.join(build_path, 'src', 'main.cpp')
    writer.write_cpp(code_s, cpp_path)
    return 0


def compile_program(args, config):
    _LOGGER.info("Compiling app...")
    build_path = relative_path(config[CONF_ESPHOMEYAML][CONF_BUILD_PATH])
    command = ['platformio', 'run', '-d', build_path]
    if args.verbose:
        command.append('-v')
    return run_platformio(*command)


def get_upload_host(config):
    if CONF_MANUAL_IP in config[CONF_WIFI]:
        host = str(config[CONF_WIFI][CONF_MANUAL_IP][CONF_STATIC_IP])
    elif CONF_HOSTNAME in config[CONF_WIFI]:
        host = config[CONF_WIFI][CONF_HOSTNAME] + config[CONF_WIFI][CONF_DOMAIN]
    else:
        host = config[CONF_ESPHOMEYAML][CONF_NAME] + config[CONF_WIFI][CONF_DOMAIN]
    return host


def upload_using_esptool(config, port):
    import esptool

    build_path = relative_path(config[CONF_ESPHOMEYAML][CONF_BUILD_PATH])
    path = os.path.join(build_path, '.pioenvs', core.NAME, 'firmware.bin')
    # pylint: disable=protected-access
    return run_platformio('esptool.py', '--before', 'default_reset', '--after', 'hard_reset',
                          '--chip', 'esp8266', '--port', port, 'write_flash', '0x0',
                          path, main=esptool._main)


def upload_program(config, args, port):
    build_path = relative_path(config[CONF_ESPHOMEYAML][CONF_BUILD_PATH])

    # if upload is to a serial port use platformio, otherwise assume ota
    serial_port = port.startswith('/') or port.startswith('COM')
    if port != 'OTA' and serial_port:
        if core.ESP_PLATFORM == ESP_PLATFORM_ESP8266 and args.use_esptoolpy:
            return upload_using_esptool(config, port)
        command = ['platformio', 'run', '-d', build_path,
                   '-t', 'upload', '--upload-port', port]
        if args.verbose:
            command.append('-v')
        return run_platformio(*command)

    if 'ota' not in config:
        _LOGGER.error("No serial port found and OTA not enabled. Can't upload!")
        return -1

    # If hostname/ip is explicitly provided as upload-port argument, use this instead of zeroconf
    # hostname. This is to support use cases where zeroconf (hostname.local) does not work.
    if port != 'OTA':
        host = port
    else:
        host = get_upload_host(config)

    from esphomeyaml.components import ota
    from esphomeyaml import espota2

    bin_file = os.path.join(build_path, '.pioenvs', core.NAME, 'firmware.bin')
    if args.host_port is not None:
        host_port = args.host_port
    else:
        host_port = int(os.getenv('ESPHOMEYAML_OTA_HOST_PORT', random.randint(10000, 60000)))

    verbose = args.verbose
    remote_port = ota.get_port(config)
    password = ota.get_auth(config)

    res = espota2.run_ota(host, remote_port, password, bin_file)
    if res == 0:
        return res
    _LOGGER.warn("OTA v2 method failed. Trying with legacy OTA...")
    return espota2.run_legacy_ota(verbose, host_port, host, remote_port, password, bin_file)


def show_logs(config, args, port, escape=False):
    serial_port = port.startswith('/') or port.startswith('COM')
    if port != 'OTA' and serial_port:
        run_miniterm(config, port, escape=escape)
        return 0
    return mqtt.show_logs(config, args.topic, args.username, args.password, args.client_id,
                          escape=escape)


def clean_mqtt(config, args):
    return mqtt.clear_topic(config, args.topic, args.username, args.password, args.client_id)


def setup_log(debug=False):
    log_level = logging.DEBUG if debug else logging.INFO
    logging.basicConfig(level=log_level)
    fmt = "%(levelname)s [%(name)s] %(message)s"
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


def strip_default_ids(config):
    value = config
    if isinstance(config, list):
        value = type(config)()
        for x in config:
            if isinstance(x, core.ID) and not x.is_manual:
                continue
            value.append(strip_default_ids(x))
        return value
    elif isinstance(config, dict):
        value = type(config)()
        for k, v in config.iteritems():
            if isinstance(v, core.ID) and not v.is_manual:
                continue
            value[k] = strip_default_ids(v)
        return value
    return value


def command_config(args, config):
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
    port = args.upload_port or choose_serial_port(config)
    exit_code = upload_program(config, args, port)
    if exit_code != 0:
        return exit_code
    _LOGGER.info(u"Successfully uploaded program.")
    return 0


def command_logs(args, config):
    port = args.serial_port or choose_serial_port(config)
    return show_logs(config, args, port, escape=args.escape)


def command_run(args, config):
    exit_code = write_cpp(config)
    if exit_code != 0:
        return exit_code
    exit_code = compile_program(args, config)
    if exit_code != 0:
        return exit_code
    _LOGGER.info(u"Successfully compiled program.")
    port = args.upload_port or choose_serial_port(config)
    exit_code = upload_program(config, args, port)
    if exit_code != 0:
        return exit_code
    _LOGGER.info(u"Successfully uploaded program.")
    if args.no_logs:
        return 0
    return show_logs(config, args, port, escape=args.escape)


def command_clean_mqtt(args, config):
    return clean_mqtt(config, args)


def command_mqtt_fingerprint(args, config):
    return mqtt.get_fingerprint(config)


def command_version(args):
    safe_print(u"Version: {}".format(const.__version__))
    return 0


def command_clean(args, config):
    build_path = relative_path(config[CONF_ESPHOMEYAML][CONF_BUILD_PATH])
    try:
        writer.clean_build(build_path)
    except OSError as err:
        _LOGGER.error("Error deleting build files: %s", err)
        return 1
    _LOGGER.info("Done!")
    return 0


def command_dashboard(args):
    from esphomeyaml.dashboard import dashboard

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
}


def parse_args(argv):
    parser = argparse.ArgumentParser(prog='esphomeyaml')
    parser.add_argument('-v', '--verbose', help="Enable verbose esphomeyaml logs.",
                        action='store_true')
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
    parser_upload.add_argument('--use-esptoolpy',
                               help="Use esptool.py for the uploading (only for ESP8266)",
                               action='store_true')

    parser_logs = subparsers.add_parser('logs', help='Validate the configuration '
                                                     'and show all MQTT logs.')
    parser_logs.add_argument('--topic', help='Manually set the topic to subscribe to.')
    parser_logs.add_argument('--username', help='Manually set the username.')
    parser_logs.add_argument('--password', help='Manually set the password.')
    parser_logs.add_argument('--client-id', help='Manually set the client id.')
    parser_logs.add_argument('--serial-port', help="Manually specify a serial port to use"
                                                   "For example /dev/cu.SLAB_USBtoUART.")
    parser_logs.add_argument('--escape', help="Escape ANSI color codes for running in dashboard",
                             action='store_true')

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
    parser_run.add_argument('--escape', help="Escape ANSI color codes for running in dashboard",
                            action='store_true')
    parser_run.add_argument('--use-esptoolpy',
                            help="Use esptool.py for the uploading (only for ESP8266)",
                            action='store_true')

    parser_clean = subparsers.add_parser('clean-mqtt', help="Helper to clear an MQTT topic from "
                                                            "retain messages.")
    parser_clean.add_argument('--topic', help='Manually set the topic to subscribe to.')
    parser_clean.add_argument('--username', help='Manually set the username.')
    parser_clean.add_argument('--password', help='Manually set the password.')
    parser_clean.add_argument('--client-id', help='Manually set the client id.')

    subparsers.add_parser('wizard', help="A helpful setup wizard that will guide "
                                         "you through setting up esphomeyaml.")

    subparsers.add_parser('mqtt-fingerprint', help="Get the SSL fingerprint from a MQTT broker.")

    subparsers.add_parser('version', help="Print the esphomeyaml version and exit.")

    subparsers.add_parser('clean', help="Delete all temporary build files.")

    dashboard = subparsers.add_parser('dashboard',
                                      help="Create a simple web server for a dashboard.")
    dashboard.add_argument("--port", help="The HTTP port to open connections on.", type=int,
                           default=6052)
    dashboard.add_argument("--password", help="The optional password to require for all requests.",
                           type=str, default='')
    dashboard.add_argument("--open-ui", help="Open the dashboard UI in a browser.",
                           action='store_true')

    return parser.parse_args(argv[1:])


def run_esphomeyaml(argv):
    args = parse_args(argv)
    setup_log(args.verbose)
    if args.command in PRE_CONFIG_ACTIONS:
        try:
            return PRE_CONFIG_ACTIONS[args.command](args)
        except ESPHomeYAMLError as e:
            _LOGGER.error(e)
            return 1

    core.CONFIG_PATH = args.configuration

    config = read_config(core.CONFIG_PATH)
    if config is None:
        return 1

    if args.command in POST_CONFIG_ACTIONS:
        try:
            return POST_CONFIG_ACTIONS[args.command](args, config)
        except ESPHomeYAMLError as e:
            _LOGGER.error(e)
            return 1
    safe_print(u"Unknown command {}".format(args.command))
    return 1


def main():
    try:
        return run_esphomeyaml(sys.argv)
    except ESPHomeYAMLError as e:
        _LOGGER.error(e)
        return 1
    except KeyboardInterrupt:
        return 1


if __name__ == "__main__":
    sys.exit(main())
