import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TIMEOUT, CONF_ESPHOME, \
    CONF_ARDUINO_VERSION, ARDUINO_VERSION_ESP8266_2_5_0
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
CONF_HEADERS = 'headers'
CONF_USERAGENT = 'useragent'
CONF_BODY = 'body'


def validate_framework(config):
    if CORE.is_esp32:
        return config

    version = 'RECOMMENDED'
    if CONF_ARDUINO_VERSION in CORE.raw_config[CONF_ESPHOME]:
        version = CORE.raw_config[CONF_ESPHOME][CONF_ARDUINO_VERSION]

    if version in ['RECOMMENDED', 'LATEST', 'DEV']:
        return config

    framework = PLATFORMIO_ESP8266_LUT[version] if version in PLATFORMIO_ESP8266_LUT else version
    if framework < ARDUINO_VERSION_ESP8266_2_5_0:
        raise cv.Invalid('This component is not supported on arduino framework version below 2.5.0')
    return config


def validate_url(value):
    value = cv.string(value)
    try:
        parsed = list(urlparse.urlparse(value))
    except Exception:
        raise cv.Invalid('Invalid URL')

    if not parsed[0] or not parsed[1]:
        raise cv.Invalid('URL must have a URL scheme and host')

    if parsed[0] not in ['http', 'https']:
        raise cv.Invalid('Scheme must be http or https')

    if not parsed[2]:
        parsed[2] = '/'

    return urlparse.urlunparse(parsed)


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(HttpRequestComponent),
    cv.Required(CONF_URL): validate_url,
    cv.Optional(CONF_METHOD, default='GET'): cv.one_of('GET', 'POST', upper=True),
    cv.Optional(CONF_HEADERS, default={}): cv.All(cv.Schema({cv.string: cv.string})),
    cv.Optional(CONF_USERAGENT): cv.string,
    cv.Optional(CONF_TIMEOUT, default='5s'): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_BODY): cv.string,
}).add_extra(validate_framework).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_url(config[CONF_URL]))
    cg.add(var.set_method(config[CONF_METHOD]))
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))

    for header in config[CONF_HEADERS]:
        cg.add(var.add_header(header, config[CONF_HEADERS][header]))

    if CONF_USERAGENT in config:
        cg.add(var.set_useragent(config[CONF_USERAGENT]))

    if CONF_BODY in config:
        cg.add(var.set_body(config[CONF_BODY]))

    yield cg.register_component(var, config)


@automation.register_action('http_request.send', HttpRequestSendAction, automation.maybe_simple_id({
    cv.Required(CONF_ID): cv.use_id(HttpRequestComponent),
    cv.Optional(CONF_BODY): cv.templatable(cv.string),
}))
def http_request_send_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_BODY in config:
        template_ = yield cg.templatable(config[CONF_BODY], args, cg.std_string)
        cg.add(var.set_body(template_))
    yield var
