import urlparse

import voluptuous as vol

from esphomeyaml import config_validation as cv
from esphomeyaml.const import CONF_ID
from esphomeyaml.helpers import App, Pvariable, esphomelib_ns

HTTPClientComponent = esphomelib_ns.HTTPClientComponent
HTTPRequestAction = esphomelib_ns.HTTPRequestAction
HTTPClientHeader = esphomelib_ns.HTTPClientHeader

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(HTTPClientComponent),
})

HTTP_METHODS = {
    'GET': esphomelib_ns.HTTP_CLIENT_METHOD_GET,
    'POST': esphomelib_ns.HTTP_CLIENT_METHOD_POST,
    'PUT': esphomelib_ns.HTTP_CLIENT_METHOD_PUT,
    'PATCH': esphomelib_ns.HTTP_CLIENT_METHOD_PATCH,
    'DELETE': esphomelib_ns.HTTP_CLIENT_METHOD_DELETE,
}


def validate_url(value):
    value = cv.string(value)
    try:
        parsed = urlparse.urlparse(value)
    except ValueError as err:
        raise vol.Invalid("Not a URL: {}".format(err))
    return parsed.geturl()


def to_code(config):
    rhs = App.make_http_client()
    Pvariable(config[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_HTTP_CLIENT'
