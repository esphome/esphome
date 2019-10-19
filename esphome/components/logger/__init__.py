import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import LambdaAction
from esphome.const import CONF_ARGS, CONF_BAUD_RATE, CONF_FORMAT, CONF_HARDWARE_UART, CONF_ID, \
    CONF_LEVEL, CONF_LOGS, CONF_TAG, CONF_TX_BUFFER_SIZE
from esphome.core import CORE, EsphomeError, Lambda, coroutine_with_priority
from esphome.py_compat import text_type

logger_ns = cg.esphome_ns.namespace('logger')
LOG_LEVELS = {
    'NONE': cg.global_ns.ESPHOME_LOG_LEVEL_NONE,
    'ERROR': cg.global_ns.ESPHOME_LOG_LEVEL_ERROR,
    'WARN': cg.global_ns.ESPHOME_LOG_LEVEL_WARN,
    'INFO': cg.global_ns.ESPHOME_LOG_LEVEL_INFO,
    'DEBUG': cg.global_ns.ESPHOME_LOG_LEVEL_DEBUG,
    'VERBOSE': cg.global_ns.ESPHOME_LOG_LEVEL_VERBOSE,
    'VERY_VERBOSE': cg.global_ns.ESPHOME_LOG_LEVEL_VERY_VERBOSE,
}

LOG_LEVEL_TO_ESP_LOG = {
    'ERROR': cg.global_ns.ESP_LOGE,
    'WARN': cg.global_ns.ESP_LOGW,
    'INFO': cg.global_ns.ESP_LOGI,
    'DEBUG': cg.global_ns.ESP_LOGD,
    'VERBOSE': cg.global_ns.ESP_LOGV,
    'VERY_VERBOSE': cg.global_ns.ESP_LOGVV,
}

LOG_LEVEL_SEVERITY = ['NONE', 'ERROR', 'WARN', 'INFO', 'CONFIG', 'DEBUG', 'VERBOSE', 'VERY_VERBOSE']

UART_SELECTION_ESP32 = ['UART0', 'UART1', 'UART2']

UART_SELECTION_ESP8266 = ['UART0', 'UART0_SWAP', 'UART1']

HARDWARE_UART_TO_UART_SELECTION = {
    'UART0': logger_ns.UART_SELECTION_UART0,
    'UART0_SWAP': logger_ns.UART_SELECTION_UART0_SWAP,
    'UART1': logger_ns.UART_SELECTION_UART1,
    'UART2': logger_ns.UART_SELECTION_UART2,
}

HARDWARE_UART_TO_SERIAL = {
    'UART0': cg.global_ns.Serial,
    'UART0_SWAP': cg.global_ns.Serial,
    'UART1': cg.global_ns.Serial1,
    'UART2': cg.global_ns.Serial2,
}

is_log_level = cv.one_of(*LOG_LEVELS, upper=True)


def uart_selection(value):
    if CORE.is_esp32:
        return cv.one_of(*UART_SELECTION_ESP32, upper=True)(value)
    if CORE.is_esp8266:
        return cv.one_of(*UART_SELECTION_ESP8266, upper=True)(value)
    raise NotImplementedError


def validate_local_no_higher_than_global(value):
    global_level = value.get(CONF_LEVEL, 'DEBUG')
    for tag, level in value.get(CONF_LOGS, {}).items():
        if LOG_LEVEL_SEVERITY.index(level) > LOG_LEVEL_SEVERITY.index(global_level):
            raise EsphomeError(u"The local log level {} for {} must be less severe than the "
                               u"global log level {}.".format(level, tag, global_level))
    return value


Logger = logger_ns.class_('Logger', cg.Component)

CONF_ESP8266_STORE_LOG_STRINGS_IN_FLASH = 'esp8266_store_log_strings_in_flash'
CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(Logger),
    cv.Optional(CONF_BAUD_RATE, default=115200): cv.positive_int,
    cv.Optional(CONF_TX_BUFFER_SIZE, default=512): cv.validate_bytes,
    cv.Optional(CONF_HARDWARE_UART, default='UART0'): uart_selection,
    cv.Optional(CONF_LEVEL, default='DEBUG'): is_log_level,
    cv.Optional(CONF_LOGS, default={}): cv.Schema({
        cv.string: is_log_level,
    }),

    cv.SplitDefault(CONF_ESP8266_STORE_LOG_STRINGS_IN_FLASH, esp8266=True):
        cv.All(cv.only_on_esp8266, cv.boolean),
}).extend(cv.COMPONENT_SCHEMA), validate_local_no_higher_than_global)


@coroutine_with_priority(90.0)
def to_code(config):
    baud_rate = config[CONF_BAUD_RATE]
    rhs = Logger.new(baud_rate,
                     config[CONF_TX_BUFFER_SIZE],
                     HARDWARE_UART_TO_UART_SELECTION[config[CONF_HARDWARE_UART]])
    log = cg.Pvariable(config[CONF_ID], rhs)
    cg.add(log.pre_setup())

    for tag, level in config[CONF_LOGS].items():
        cg.add(log.set_log_level(tag, LOG_LEVELS[level]))

    level = config[CONF_LEVEL]
    cg.add_define('USE_LOGGER')
    this_severity = LOG_LEVEL_SEVERITY.index(level)
    cg.add_build_flag('-DESPHOME_LOG_LEVEL={}'.format(LOG_LEVELS[level]))

    verbose_severity = LOG_LEVEL_SEVERITY.index('VERBOSE')
    very_verbose_severity = LOG_LEVEL_SEVERITY.index('VERY_VERBOSE')
    is_at_least_verbose = this_severity >= verbose_severity
    is_at_least_very_verbose = this_severity >= very_verbose_severity
    has_serial_logging = baud_rate != 0

    if CORE.is_esp8266 and has_serial_logging and is_at_least_verbose:
        debug_serial_port = HARDWARE_UART_TO_SERIAL[config.get(CONF_HARDWARE_UART)]
        cg.add_build_flag("-DDEBUG_ESP_PORT={}".format(debug_serial_port))
        cg.add_build_flag("-DLWIP_DEBUG")
        DEBUG_COMPONENTS = {
            'HTTP_CLIENT',
            'HTTP_SERVER',
            'HTTP_UPDATE',
            'OTA',
            'SSL',
            'TLS_MEM',
            'UPDATER',
            'WIFI',
            # Spams logs too much:
            # 'MDNS_RESPONDER',
        }
        for comp in DEBUG_COMPONENTS:
            cg.add_build_flag("-DDEBUG_ESP_{}".format(comp))
    if CORE.is_esp32 and is_at_least_verbose:
        cg.add_build_flag('-DCORE_DEBUG_LEVEL=5')
    if CORE.is_esp32 and is_at_least_very_verbose:
        cg.add_build_flag('-DENABLE_I2C_DEBUG_BUFFER')
    if config.get(CONF_ESP8266_STORE_LOG_STRINGS_IN_FLASH):
        cg.add_build_flag('-DUSE_STORE_LOG_STR_IN_FLASH')

    # Register at end for safe mode
    yield cg.register_component(log, config)


def maybe_simple_message(schema):
    def validator(value):
        if isinstance(value, dict):
            return cv.Schema(schema)(value)
        return cv.Schema(schema)({CONF_FORMAT: value})

    return validator


def validate_printf(value):
    # https://stackoverflow.com/questions/30011379/how-can-i-parse-a-c-format-string-in-python
    # pylint: disable=anomalous-backslash-in-string
    cfmt = u"""\
    (                                  # start of capture group 1
    %                                  # literal "%"
    (?:                                # first option
    (?:[-+0 #]{0,5})                   # optional flags
    (?:\d+|\*)?                        # width
    (?:\.(?:\d+|\*))?                  # precision
    (?:h|l|ll|w|I|I32|I64)?            # size
    [cCdiouxXeEfgGaAnpsSZ]             # type
    ) |                                # OR
    %%)                                # literal "%%"
    """  # noqa
    matches = re.findall(cfmt, value[CONF_FORMAT], flags=re.X)
    if len(matches) != len(value[CONF_ARGS]):
        raise cv.Invalid(u"Found {} printf-patterns ({}), but {} args were given!"
                         u"".format(len(matches), u', '.join(matches), len(value[CONF_ARGS])))
    return value


CONF_LOGGER_LOG = 'logger.log'
LOGGER_LOG_ACTION_SCHEMA = cv.All(maybe_simple_message({
    cv.Required(CONF_FORMAT): cv.string,
    cv.Optional(CONF_ARGS, default=list): cv.ensure_list(cv.lambda_),
    cv.Optional(CONF_LEVEL, default="DEBUG"): cv.one_of(*LOG_LEVEL_TO_ESP_LOG, upper=True),
    cv.Optional(CONF_TAG, default="main"): cv.string,
}), validate_printf)


@automation.register_action(CONF_LOGGER_LOG, LambdaAction, LOGGER_LOG_ACTION_SCHEMA)
def logger_log_action_to_code(config, action_id, template_arg, args):
    esp_log = LOG_LEVEL_TO_ESP_LOG[config[CONF_LEVEL]]
    args_ = [cg.RawExpression(text_type(x)) for x in config[CONF_ARGS]]

    text = text_type(cg.statement(esp_log(config[CONF_TAG], config[CONF_FORMAT], *args_)))

    lambda_ = yield cg.process_lambda(Lambda(text), args, return_type=cg.void)
    yield cg.new_Pvariable(action_id, template_arg, lambda_)
