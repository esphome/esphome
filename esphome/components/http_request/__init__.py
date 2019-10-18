import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID, CONF_TIMEOUT, CONF_ESPHOME, CONF_ARDUINO_VERSION, ARDUINO_VERSION_ESP8266_DEV,
    ARDUINO_VERSION_ESP8266_2_5_0, ARDUINO_VERSION_ESP8266_2_5_1, ARDUINO_VERSION_ESP8266_2_5_2
)
from esphome.core import CORE
from esphome.core_config import PLATFORMIO_ESP8266_LUT
from esphome.py_compat import IS_PY3

if IS_PY3:
    import urllib.parse as urlparse  # pylint: disable=no-name-in-module,import-error
else:
    import urlparse  # pylint: disable=import-error

DEPENDENCIES = ['network']

http_request_ns = cg.esphome_ns.namespace('http_request')
HttpRequestComponent = http_request_ns.class_('HttpRequestComponent', cg.Component)
HttpRequestSendAction = http_request_ns.class_('HttpRequestSendAction', automation.Action)
MULTI_CONF = True

CONF_URL = 'url'
CONF_METHOD = 'method'
CONF_SSL_FINGERPRINT = 'ssl_fingerprint'
CONF_HEADERS = 'headers'
CONF_USERAGENT = 'useragent'
CONF_PAYLOAD = 'payload'


def _is_new_framework():
    if not CORE.is_esp8266:
        return False

    frameworks = [
        ARDUINO_VERSION_ESP8266_DEV, ARDUINO_VERSION_ESP8266_2_5_0,
        ARDUINO_VERSION_ESP8266_2_5_1, ARDUINO_VERSION_ESP8266_2_5_2
    ]
    version = 'RECOMMENDED'

    if CONF_ARDUINO_VERSION in CORE.raw_config[CONF_ESPHOME]:
        version = CORE.raw_config[CONF_ESPHOME][CONF_ARDUINO_VERSION]

    framework = PLATFORMIO_ESP8266_LUT[version]
    if framework in frameworks:
        return True

    return False


def _is_ssl_no_fingerprint(config):
    ssl = config[CONF_URL].startswith('https')
    return ssl and CONF_SSL_FINGERPRINT not in config


def _url_fingerprint_validator(config):
    if not CORE.is_esp8266 or CONF_URL not in config:
        return config

    if _is_ssl_no_fingerprint(config) and not _is_new_framework():
        raise cv.Invalid("'ssl_fingerprint' is required for HTTPS urls "
                         "on arduino framework version below 2.5.0")
    return config


def _ssl_fingerprint(value):
    value = cv.string(value)

    if re.match(r'^([0-9a-f]{2}[: ]){19}[0-9a-f]{2}$', value, re.IGNORECASE) is None:
        raise cv.Invalid("SSL Fingerprint must consist of 20 colon or whitespace "
                         "separated hexadecimal parts from 00 to FF")
    return value


def _url(value):
    value = cv.string(value)
    try:
        parsed = list(urlparse.urlparse(value))
    except Exception:
        raise cv.Invalid("Invalid URL")

    if not parsed[0] or not parsed[1]:
        raise cv.Invalid("URL must have a URL scheme and host")

    if parsed[0] not in ['http', 'https']:
        raise cv.Invalid("Scheme must be http or https")

    if not parsed[2]:
        parsed[2] = '/'

    return urlparse.urlunparse(parsed)


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(HttpRequestComponent),
    cv.Required(CONF_URL): _url,
    cv.Optional(CONF_METHOD, default='GET'): cv.one_of('GET', 'POST', upper=True),
    cv.Optional(CONF_SSL_FINGERPRINT): cv.All(cv.only_on_esp8266, _ssl_fingerprint),
    cv.Optional(CONF_HEADERS, default={}): cv.All(cv.Schema({cv.string: cv.string})),
    cv.Optional(CONF_USERAGENT): cv.string,
    cv.Optional(CONF_TIMEOUT, default='5s'): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_PAYLOAD): cv.string,
}).add_extra(_url_fingerprint_validator).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    if _is_new_framework() and _is_ssl_no_fingerprint(config):
        cg.add_define('HTTP_CLIENT_SUPPORT_INSECURE')

    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_url(config[CONF_URL]))
    cg.add(var.set_method(config[CONF_METHOD]))
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))

    if CONF_SSL_FINGERPRINT in config:
        arr = [cg.RawExpression("0x{}".format(
            config[CONF_SSL_FINGERPRINT][i:i + 2])) for i in range(0, 59, 3)]
        cg.add(var.set_ssl_fingerprint(arr))

    for header in config[CONF_HEADERS]:
        cg.add(var.add_header(header, config[CONF_HEADERS][header]))

    if CONF_USERAGENT in config:
        cg.add(var.set_useragent(config[CONF_USERAGENT]))

    if CONF_PAYLOAD in config:
        cg.add(var.set_payload(config[CONF_PAYLOAD]))

    yield cg.register_component(var, config)


@automation.register_action('http_request.send', HttpRequestSendAction, automation.maybe_simple_id({
    cv.Required(CONF_ID): cv.use_id(HttpRequestComponent),
    cv.Optional(CONF_PAYLOAD): cv.templatable(cv.string),
}))
def http_request_send_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_PAYLOAD in config:
        template_ = yield cg.templatable(config[CONF_PAYLOAD], args, cg.std_string)
        cg.add(var.set_payload(template_))
    yield var
