import logging
import mimetypes
import os.path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import (
    CONF_CSS_INCLUDE,
    CONF_CSS_URL,
    CONF_ID,
    CONF_JS_INCLUDE,
    CONF_JS_URL,
    CONF_PORT,
    CONF_AUTH,
    CONF_USERNAME,
    CONF_PASSWORD,
    CONF_INCLUDE_INTERNAL,
    CONF_OTA,
    CONF_VERSION,
    CONF_LOCAL,
    CONF_SOURCE,
)
from esphome.core import CORE, coroutine_with_priority, HexInt


AUTO_LOAD = ["json", "web_server_base"]

web_server_ns = cg.esphome_ns.namespace("web_server")
WebServer = web_server_ns.class_("WebServer", cg.Component, cg.Controller)

CONF_APP = "app"

_LOGGER = logging.getLogger(__name__)


def default_url(config):
    config = config.copy()
    if config[CONF_VERSION] == 1:
        if not (CONF_CSS_URL in config):
            config[CONF_CSS_URL] = "https://esphome.io/_static/webserver-v1.min.css"
        if not (CONF_JS_URL in config):
            config[CONF_JS_URL] = "https://esphome.io/_static/webserver-v1.min.js"
    if config[CONF_VERSION] == 2:
        if not (CONF_CSS_URL in config):
            config[CONF_CSS_URL] = ""
        if not (CONF_JS_URL in config):
            config[CONF_JS_URL] = "https://oi.esphome.io/v2/www.js"
    return config


def validate_local(config):
    if CONF_LOCAL in config and config[CONF_VERSION] == 1:
        raise cv.Invalid("'local' is not supported in version 1")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebServer),
            cv.Optional(CONF_PORT, default=80): cv.port,
            cv.Optional(CONF_VERSION, default=2): cv.one_of(1, 2),
            cv.Optional(CONF_CSS_URL): cv.string,
            cv.Optional(CONF_CSS_INCLUDE): cv.file_,
            cv.Optional(CONF_JS_URL): cv.string,
            cv.Optional(CONF_JS_INCLUDE): cv.file_,
            cv.Optional(CONF_AUTH): cv.Schema(
                {
                    cv.Required(CONF_USERNAME): cv.All(
                        cv.string_strict, cv.Length(min=1)
                    ),
                    cv.Required(CONF_PASSWORD): cv.All(
                        cv.string_strict, cv.Length(min=1)
                    ),
                }
            ),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.Optional(CONF_INCLUDE_INTERNAL, default=False): cv.boolean,
            cv.Optional(CONF_OTA, default=True): cv.boolean,
            cv.Optional(CONF_LOCAL): cv.boolean,
            cv.Optional(CONF_APP): cv.Schema(
                {
                    cv.Required(CONF_SOURCE): cv.directory,
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
    default_url,
    validate_local,
)


@coroutine_with_priority(40.0)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    cg.add_define("USE_WEBSERVER")

    cg.add(paren.set_port(config[CONF_PORT]))
    cg.add_define("USE_WEBSERVER")
    cg.add_define("USE_WEBSERVER_PORT", config[CONF_PORT])
    cg.add_define("USE_WEBSERVER_VERSION", config[CONF_VERSION])
    cg.add(var.set_css_url(config[CONF_CSS_URL]))
    cg.add(var.set_js_url(config[CONF_JS_URL]))
    cg.add(var.set_allow_ota(config[CONF_OTA]))
    if CONF_AUTH in config:
        cg.add(paren.set_auth_username(config[CONF_AUTH][CONF_USERNAME]))
        cg.add(paren.set_auth_password(config[CONF_AUTH][CONF_PASSWORD]))
    if CONF_CSS_INCLUDE in config:
        cg.add_define("USE_WEBSERVER_CSS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_CSS_INCLUDE])
        with open(file=path, encoding="utf-8") as myfile:
            cg.add(var.set_css_include(myfile.read()))
    if CONF_JS_INCLUDE in config:
        cg.add_define("USE_WEBSERVER_JS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_JS_INCLUDE])
        with open(file=path, encoding="utf-8") as myfile:
            cg.add(var.set_js_include(myfile.read()))
    cg.add(var.set_include_internal(config[CONF_INCLUDE_INTERNAL]))
    if CONF_LOCAL in config and config[CONF_LOCAL]:
        cg.add_define("USE_WEBSERVER_LOCAL")
    if CONF_APP in config:
        add_web_app(config, var)


def add_web_app(config, var):
    """Embed web application files into flash and configure the web server to serve them.

    For each file in the web app source directory:
    - detect file MIME type based on file extension (e.g. text/html)
    - detect transfer encoding type for compressed files based on file extension (e.g. gzip, compress, bzip2)
    - embed file contents as PROGMEM byte stream
    - add file descriptor to the list of web app files served by the web server

    See https://docs.python.org/3.8/library/mimetypes.html for internals of file type and encoding detection logic.
    """
    mimetypes.init()
    cg.add_define("USE_WEBSERVER_APP")
    app_config = config[CONF_APP]
    source_dir = CORE.relative_config_path(app_config[CONF_SOURCE])

    # add all files in source directory
    file_index = 0
    for file_name in os.listdir(source_dir):
        file_path = os.path.join(source_dir, file_name)
        # no recursion, process files only
        if os.path.isfile(file_path):
            # guess file MIME type & encoding
            mime_type = mimetypes.guess_type(file_path, False)
            file_type = mime_type[0]  # e.g. text/html, application/javascript or None
            # fallback to application/octet-stream for unknown MIME types
            if file_type is None:
                _LOGGER.warning(
                    "Could not detect MIME type for file %s. Using application/octet-stream.",
                    file_path,
                )
                file_type = "application/octet-stream"
            # handle file encoding (e.g. gzip, compress, ...)
            file_encoding = mime_type[1]  # e.g. gzip, compress or None
            if file_encoding is None:
                file_encoding = "none"
            else:
                file_name = os.path.splitext(file_name)[
                    0
                ]  # remove compressed file extension
            # add the file to the list of web server app files
            with open(file=file_path, mode="rb") as content:
                data = content.read()
                _LOGGER.info(
                    "Adding web app file: %s as app/%s (size %d, %s, encoding %s)",
                    file_path,
                    file_name,
                    len(data),
                    file_type,
                    file_encoding,
                )
                rhs = [HexInt(x) for x in data]
                prog_arr = cg.progmem_array(
                    core.ID(id="web_server_app_file" + str(file_index), type=cg.uint8),
                    rhs,
                )
                cg.add(
                    var.add_app_file(
                        file_name, file_type, file_encoding, prog_arr, len(data)
                    )
                )
                file_index += 1
