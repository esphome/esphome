from __future__ import annotations

import gzip

import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTH,
    CONF_CSS_INCLUDE,
    CONF_CSS_URL,
    CONF_ENABLE_PRIVATE_NETWORK_ACCESS,
    CONF_ID,
    CONF_INCLUDE_INTERNAL,
    CONF_JS_INCLUDE,
    CONF_JS_URL,
    CONF_LOCAL,
    CONF_LOG,
    CONF_NAME,
    CONF_OTA,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_USERNAME,
    CONF_VERSION,
    CONF_WEB_SERVER,
    CONF_WEB_SERVER_ID,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RTL87XX,
)
from esphome.core import CORE, coroutine_with_priority
import esphome.final_validate as fv

AUTO_LOAD = ["json", "web_server_base"]

CONF_SORTING_GROUP_ID = "sorting_group_id"
CONF_SORTING_GROUPS = "sorting_groups"
CONF_SORTING_WEIGHT = "sorting_weight"

web_server_ns = cg.esphome_ns.namespace("web_server")
WebServer = web_server_ns.class_("WebServer", cg.Component, cg.Controller)

sorting_groups = {}


def default_url(config):
    config = config.copy()
    if config[CONF_VERSION] == 1:
        if CONF_CSS_URL not in config:
            config[CONF_CSS_URL] = "https://esphome.io/_static/webserver-v1.min.css"
        if CONF_JS_URL not in config:
            config[CONF_JS_URL] = "https://esphome.io/_static/webserver-v1.min.js"
    if config[CONF_VERSION] == 2:
        if CONF_CSS_URL not in config:
            config[CONF_CSS_URL] = ""
        if CONF_JS_URL not in config:
            config[CONF_JS_URL] = "https://oi.esphome.io/v2/www.js"
    if config[CONF_VERSION] == 3:
        if CONF_CSS_URL not in config:
            config[CONF_CSS_URL] = ""
        if CONF_JS_URL not in config:
            config[CONF_JS_URL] = "https://oi.esphome.io/v3/www.js"
    return config


def validate_local(config):
    if CONF_LOCAL in config and config[CONF_VERSION] == 1:
        raise cv.Invalid("'local' is not supported in version 1")
    return config


def validate_ota(config):
    if CORE.using_esp_idf and config[CONF_OTA]:
        raise cv.Invalid("Enabling 'ota' is not supported for IDF framework yet")
    return config


def validate_sorting_groups(config):
    if CONF_SORTING_GROUPS in config and config[CONF_VERSION] != 3:
        raise cv.Invalid(
            f"'{CONF_SORTING_GROUPS}' is only supported in 'web_server' version 3"
        )
    return config


def _validate_no_sorting_component(
    sorting_component: str,
    webserver_version: int,
    config: dict,
    path: list[str] | None = None,
) -> None:
    if path is None:
        path = []
    if CONF_WEB_SERVER in config and sorting_component in config[CONF_WEB_SERVER]:
        raise cv.FinalExternalInvalid(
            f"{sorting_component} on entities is not supported in web_server version {webserver_version}",
            path=path + [sorting_component],
        )
    for p, value in config.items():
        if isinstance(value, dict):
            _validate_no_sorting_component(
                sorting_component, webserver_version, value, path + [p]
            )
        elif isinstance(value, list):
            for i, item in enumerate(value):
                if isinstance(item, dict):
                    _validate_no_sorting_component(
                        sorting_component, webserver_version, item, path + [p, i]
                    )


def _final_validate_sorting(config):
    if (webserver_version := config.get(CONF_VERSION)) != 3:
        _validate_no_sorting_component(
            CONF_SORTING_WEIGHT, webserver_version, fv.full_config.get()
        )
        _validate_no_sorting_component(
            CONF_SORTING_GROUP_ID, webserver_version, fv.full_config.get()
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate_sorting

sorting_group = {
    cv.Required(CONF_ID): cv.declare_id(cg.int_),
    cv.Required(CONF_NAME): cv.string,
    cv.Optional(CONF_SORTING_WEIGHT): cv.float_,
}

WEBSERVER_SORTING_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WEB_SERVER): cv.Schema(
            {
                cv.OnlyWith(CONF_WEB_SERVER_ID, "web_server"): cv.use_id(WebServer),
                cv.Optional(CONF_SORTING_WEIGHT): cv.All(
                    cv.requires_component("web_server"),
                    cv.float_,
                ),
                cv.Optional(CONF_SORTING_GROUP_ID): cv.All(
                    cv.requires_component("web_server"),
                    cv.use_id(cg.int_),
                ),
            }
        )
    }
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebServer),
            cv.Optional(CONF_PORT, default=80): cv.port,
            cv.Optional(CONF_VERSION, default=2): cv.one_of(1, 2, 3, int=True),
            cv.Optional(CONF_CSS_URL): cv.string,
            cv.Optional(CONF_CSS_INCLUDE): cv.file_,
            cv.Optional(CONF_JS_URL): cv.string,
            cv.Optional(CONF_JS_INCLUDE): cv.file_,
            cv.Optional(CONF_ENABLE_PRIVATE_NETWORK_ACCESS, default=True): cv.boolean,
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
            cv.SplitDefault(
                CONF_OTA,
                esp8266=True,
                esp32_arduino=True,
                esp32_idf=False,
                bk72xx=True,
                rtl87xx=True,
            ): cv.boolean,
            cv.Optional(CONF_LOG, default=True): cv.boolean,
            cv.Optional(CONF_LOCAL): cv.boolean,
            cv.Optional(CONF_SORTING_GROUPS): cv.ensure_list(sorting_group),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_BK72XX, PLATFORM_RTL87XX]),
    default_url,
    validate_local,
    validate_ota,
    validate_sorting_groups,
)


def add_sorting_groups(web_server_var, config):
    for group in config:
        sorting_groups[group[CONF_ID]] = group[CONF_NAME]
        group_sorting_weight = group.get(CONF_SORTING_WEIGHT, 50)
        cg.add(
            web_server_var.add_sorting_group(
                hash(group[CONF_ID]), group[CONF_NAME], group_sorting_weight
            )
        )


async def add_entity_config(entity, config):
    web_server = await cg.get_variable(config[CONF_WEB_SERVER_ID])
    sorting_weight = config.get(CONF_SORTING_WEIGHT, 50)
    sorting_group_hash = hash(config.get(CONF_SORTING_GROUP_ID))

    cg.add(
        web_server.add_entity_config(
            entity,
            sorting_weight,
            sorting_group_hash,
        )
    )


def build_index_html(config) -> str:
    html = "<!DOCTYPE html><html><head><meta charset=UTF-8><link rel=icon href=data:>"
    css_include = config.get(CONF_CSS_INCLUDE)
    js_include = config.get(CONF_JS_INCLUDE)
    if css_include:
        html += "<link rel=stylesheet href=/0.css>"
    if config[CONF_CSS_URL]:
        html += f'<link rel=stylesheet href="{config[CONF_CSS_URL]}">'
    html += "</head><body>"
    if js_include:
        html += "<script type=module src=/0.js></script>"
    html += "<esp-app></esp-app>"
    if config[CONF_JS_URL]:
        html += f'<script src="{config[CONF_JS_URL]}"></script>'
    html += "</body></html>"
    return html


def add_resource_as_progmem(
    resource_name: str, content: str, compress: bool = True
) -> None:
    """Add a resource to progmem."""
    content_encoded = content.encode("utf-8")
    if compress:
        content_encoded = gzip.compress(content_encoded)
    content_encoded_size = len(content_encoded)
    bytes_as_int = ", ".join(str(x) for x in content_encoded)
    uint8_t = f"const uint8_t ESPHOME_WEBSERVER_{resource_name}[{content_encoded_size}] PROGMEM = {{{bytes_as_int}}}"
    size_t = (
        f"const size_t ESPHOME_WEBSERVER_{resource_name}_SIZE = {content_encoded_size}"
    )
    cg.add_global(cg.RawExpression(uint8_t))
    cg.add_global(cg.RawExpression(size_t))


@coroutine_with_priority(40.0)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    version = config[CONF_VERSION]

    cg.add(paren.set_port(config[CONF_PORT]))
    cg.add_define("USE_WEBSERVER")
    cg.add_define("USE_WEBSERVER_PORT", config[CONF_PORT])
    cg.add_define("USE_WEBSERVER_VERSION", version)
    if version >= 2:
        # Don't compress the index HTML as the data sizes are almost the same.
        add_resource_as_progmem("INDEX_HTML", build_index_html(config), compress=False)
    else:
        cg.add(var.set_css_url(config[CONF_CSS_URL]))
        cg.add(var.set_js_url(config[CONF_JS_URL]))
    cg.add(var.set_allow_ota(config[CONF_OTA]))
    cg.add(var.set_expose_log(config[CONF_LOG]))
    if config[CONF_ENABLE_PRIVATE_NETWORK_ACCESS]:
        cg.add_define("USE_WEBSERVER_PRIVATE_NETWORK_ACCESS")
    if CONF_AUTH in config:
        cg.add(paren.set_auth_username(config[CONF_AUTH][CONF_USERNAME]))
        cg.add(paren.set_auth_password(config[CONF_AUTH][CONF_PASSWORD]))
    if CONF_CSS_INCLUDE in config:
        cg.add_define("USE_WEBSERVER_CSS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_CSS_INCLUDE])
        with open(file=path, encoding="utf-8") as css_file:
            add_resource_as_progmem("CSS_INCLUDE", css_file.read())
    if CONF_JS_INCLUDE in config:
        cg.add_define("USE_WEBSERVER_JS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_JS_INCLUDE])
        with open(file=path, encoding="utf-8") as js_file:
            add_resource_as_progmem("JS_INCLUDE", js_file.read())
    cg.add(var.set_include_internal(config[CONF_INCLUDE_INTERNAL]))
    if CONF_LOCAL in config and config[CONF_LOCAL]:
        cg.add_define("USE_WEBSERVER_LOCAL")

    if (sorting_group_config := config.get(CONF_SORTING_GROUPS)) is not None:
        add_sorting_groups(var, sorting_group_config)
