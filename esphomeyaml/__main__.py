from __future__ import print_function

import argparse
import logging
import os
import random
import sys

from esphomeyaml import core, mqtt, wizard, writer, yaml_util, const
from esphomeyaml.config import core_to_code, get_component, iter_components, read_config
from esphomeyaml.const import CONF_BAUD_RATE, CONF_ESPHOMEYAML, CONF_HOSTNAME, CONF_LOGGER, \
    CONF_MANUAL_IP, CONF_NAME, CONF_STATIC_IP, CONF_WIFI
from esphomeyaml.helpers import AssignmentExpression, RawStatement, _EXPRESSIONS, add, add_task, \
    color, get_variable, indent, quote, statement, Expression

_LOGGER = logging.getLogger(__name__)

PRE_INITIALIZE = ['esphomeyaml', 'logger', 'wifi', 'ota', 'mqtt', 'web_server', 'i2c']


def get_name(config):
    return config[CONF_ESPHOMEYAML][CONF_NAME]


def get_base_path(config):
    return os.path.join(os.path.dirname(core.CONFIG_PATH), get_name(config))


def discover_serial_ports():
    # from https://github.com/pyserial/pyserial/blob/master/serial/tools/list_ports.py
    try:
        from serial.tools.list_ports import comports
    except ImportError:
        return None

    result = []
    descs = []
    for port, desc, info in comports():
        if not port:
            continue
        if "VID:PID" in info:
            result.append(port)
            descs.append(desc)

    if not result:
        return None
    print(u"Found multiple serial port options, please choose one:")
    for i, (res, desc) in enumerate(zip(result, descs)):
        print(u"  [{}] {} ({})".format(i, res, desc))
    print(u"  [{}] Over The Air".format(len(result)))
    print()
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
            print(color('red', u"Invalid option: '{}'".format(opt)))
    if opt == len(result):
        return None
    return result[opt]


def run_platformio(*cmd):
    def mock_exit(return_code):
        raise SystemExit(return_code)

    orig_argv = sys.argv
    orig_exit = sys.exit  # mock sys.exit
    full_cmd = u' '.join(quote(x) for x in cmd)
    _LOGGER.info(u"Running:  %s", full_cmd)
    try:
        import platformio.__main__
        sys.argv = list(cmd)
        sys.exit = mock_exit
        return platformio.__main__.main()
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


def run_miniterm(config, port):
    from serial.tools import miniterm
    baud_rate = config.get(CONF_LOGGER, {}).get(CONF_BAUD_RATE, 115200)
    sys.argv = ['miniterm', '--raw', '--exit-char', '3']
    miniterm.main(
        default_port=port,
        default_baudrate=baud_rate)


def write_cpp(config):
    _LOGGER.info("Generating C++ source...")

    add_task(core_to_code, config[CONF_ESPHOMEYAML])
    for domain in PRE_INITIALIZE:
        if domain == CONF_ESPHOMEYAML:
            continue
        if domain in config:
            add_task(get_component(domain).to_code, config[domain])

    # Clear queue
    get_variable(None)
    add(RawStatement(''))

    for domain, component, conf in iter_components(config):
        if domain in PRE_INITIALIZE:
            continue
        if not hasattr(component, 'to_code'):
            continue
        add_task(component.to_code, conf)

    # Clear queue
    get_variable(None)
    add(RawStatement(''))
    add(RawStatement(''))

    all_code = []
    for exp in _EXPRESSIONS:
        if core.SIMPLIFY:
            if isinstance(exp, Expression) and not exp.required:
                continue
            if isinstance(exp, AssignmentExpression) and not exp.obj.required:
                exp = exp.rhs
        all_code.append(unicode(statement(exp)))

    platformio_ini_s = writer.get_ini_content(config)
    ini_path = os.path.join(get_base_path(config), 'platformio.ini')
    writer.write_platformio_ini(platformio_ini_s, ini_path)

    code_s = indent('\n'.join(line.rstrip() for line in all_code))
    cpp_path = os.path.join(get_base_path(config), 'src', 'main.cpp')
    writer.write_cpp(code_s, cpp_path)
    return 0


def compile_program(config):
    _LOGGER.info("Compiling app...")
    return run_platformio('platformio', 'run', '-d', get_base_path(config))


def upload_program(config, args, port):
    _LOGGER.info("Uploading binary...")
    if port is not None:
        return run_platformio('platformio', 'run', '-d', get_base_path(config),
                              '-t', 'upload', '--upload-port', port)

    if CONF_MANUAL_IP in config[CONF_WIFI]:
        host = str(config[CONF_WIFI][CONF_MANUAL_IP][CONF_STATIC_IP])
    elif CONF_HOSTNAME in config[CONF_WIFI]:
        host = config[CONF_WIFI][CONF_HOSTNAME] + u'.local'
    else:
        host = config[CONF_ESPHOMEYAML][CONF_NAME] + u'.local'

    from esphomeyaml.components import ota
    from esphomeyaml import espota

    bin_file = os.path.join(get_base_path(config), '.pioenvs', get_name(config), 'firmware.bin')
    if args.host_port is not None:
        host_port = args.host_port
    else:
        host_port = int(os.getenv('ESPHOMEYAML_OTA_HOST_PORT', random.randint(10000, 60000)))
    espota_args = ['espota.py', '--debug', '--progress', '-i', host,
                   '-p', str(ota.get_port(config)), '-f', bin_file,
                   '-a', ota.get_auth(config), '-P', str(host_port)]
    return espota.main(espota_args)


def show_logs(config, args, port):
    if port is not None and port != 'OTA':
        run_miniterm(config, port)
        return 0
    return mqtt.show_logs(config, args.topic, args.username, args.password, args.client_id)


def clean_mqtt(config, args):
    return mqtt.clear_topic(config, args.topic, args.username, args.password, args.client_id)


def setup_log():
    logging.basicConfig(level=logging.INFO)
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


def main():
    setup_log()

    parser = argparse.ArgumentParser(prog='esphomeyaml')
    parser.add_argument('configuration', help='Your YAML configuration file.')
    subparsers = parser.add_subparsers(help='Commands', dest='command')
    subparsers.required = True
    subparsers.add_parser('config', help='Validate the configuration and spit it out.')

    subparsers.add_parser('compile', help='Read the configuration and compile a program.')

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
    parser_run.add_argument('--upload-port', help="Manually specify the upload port to use. "
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
                                         "you through setting up esphomeyaml.")

    subparsers.add_parser('mqtt-fingerprint', help="Get the SSL fingerprint from a MQTT broker.")
    subparsers.add_parser('version', help="Print the esphomeyaml version and exit.")

    args = parser.parse_args()

    if args.command == 'wizard':
        return wizard.wizard(args.configuration)

    core.CONFIG_PATH = args.configuration

    config = read_config(core.CONFIG_PATH)
    if config is None:
        return 1

    if args.command == 'config':
        print(yaml_util.dump(config))
        return 0
    elif args.command == 'compile':
        exit_code = write_cpp(config)
        if exit_code != 0:
            return exit_code
        exit_code = compile_program(config)
        if exit_code != 0:
            return exit_code
        _LOGGER.info(u"Successfully compiled program.")
        return 0
    elif args.command == 'upload':
        port = args.upload_port or discover_serial_ports()
        exit_code = upload_program(config, args, port)
        if exit_code != 0:
            return exit_code
        _LOGGER.info(u"Successfully uploaded program.")
        return 0
    elif args.command == 'logs':
        port = args.serial_port or discover_serial_ports()
        return show_logs(config, args, port)
    elif args.command == 'clean-mqtt':
        return clean_mqtt(config, args)
    elif args.command == 'mqtt-fingerprint':
        return mqtt.get_fingerprint(config)
    elif args.command == 'run':
        exit_code = write_cpp(config)
        if exit_code != 0:
            return exit_code
        exit_code = compile_program(config)
        if exit_code != 0:
            return exit_code
        _LOGGER.info(u"Successfully compiled program.")
        port = args.upload_port or discover_serial_ports()
        exit_code = upload_program(config, args, port)
        if exit_code != 0:
            return exit_code
        _LOGGER.info(u"Successfully uploaded program.")
        if args.no_logs:
            return 0
        return show_logs(config, args, port)
    elif args.command == 'version':
        print(u"Version: {}".format(const.__version__))
        return 0
    print(u"Unknown command {}".format(args.command))
    return 1


if __name__ == "__main__":
    sys.exit(main())
