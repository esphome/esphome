import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TIMEOUT

DEPENDENCIES = ['network']

http_request_ns = cg.esphome_ns.namespace('http_request')
HttpRequestComponent = http_request_ns.class_('HttpRequestComponent', cg.Component)
HttpRequestSendAction = http_request_ns.class_('HttpRequestSendAction', automation.Action)
MULTI_CONF = True

CONF_URI = 'uri'
CONF_METHOD = 'method'
CONF_SSL_FINGERPRINT = 'ssl_fingerprint'
CONF_HEADERS = 'headers'
CONF_USERAGENT = 'useragent'
CONF_PAYLOAD = 'payload'


def ssl_fingerprint(value):
    value = cv.string(value)
    if re.match(r'^([0-9a-f]{2}[: ]){19}[0-9a-f]{2}$', value, re.IGNORECASE) is None:
        raise cv.Invalid("SSL Fingerprint must consist of 20 colon or whitespace "
                         "separated hexadecimal parts from 00 to FF")
    return value


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(HttpRequestComponent),
    cv.Required(CONF_URI): cv.string,
    cv.Optional(CONF_METHOD, default='GET'): cv.one_of('GET', 'POST', upper=True),
    cv.Optional(CONF_SSL_FINGERPRINT): ssl_fingerprint,
    cv.Optional(CONF_HEADERS, default={}): cv.All(cv.Schema({cv.string: cv.string})),
    cv.Optional(CONF_USERAGENT): cv.string,
    cv.Optional(CONF_TIMEOUT, default='5s'): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_PAYLOAD): cv.string,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_uri(config[CONF_URI]))
    cg.add(var.set_method(config[CONF_METHOD]))
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))

    if CONF_SSL_FINGERPRINT in config:
        arr = [cg.RawExpression("0x{}".format(config[CONF_SSL_FINGERPRINT][i:i + 2])) for i in range(0, 59, 3)]
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
}))
def http_request_send_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)
