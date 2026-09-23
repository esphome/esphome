from __future__ import annotations

import gzip
import logging
import re

import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.logger import request_log_listener
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTH,
    CONF_COMPRESSION,
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
    CONF_TYPE,
    CONF_USERNAME,
    CONF_VERSION,
    CONF_WEB_SERVER,
    CONF_WEB_SERVER_ID,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RP2,
    PLATFORM_RTL87XX,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["json", "web_server_base"]

AUTH_TYPE_BASIC = "basic"
AUTH_TYPE_DIGEST = "digest"

CONF_SORTING_GROUP_ID = "sorting_group_id"
CONF_SORTING_GROUPS = "sorting_groups"
CONF_SORTING_WEIGHT = "sorting_weight"
CONF_ALLOWED_ORIGINS = "allowed_origins"


web_server_ns = cg.esphome_ns.namespace("web_server")
WebServer = web_server_ns.class_("WebServer", cg.Component, cg.Controller)

sorting_groups = {}


def default_url(config: ConfigType) -> ConfigType:
    config = config.copy()
    if config[CONF_VERSION] == 1:
        if CONF_CSS_URL not in config:
            config[CONF_CSS_URL] = "https://oi.esphome.io/v1/webserver-v1.min.css"
        if CONF_JS_URL not in config:
            config[CONF_JS_URL] = "https://oi.esphome.io/v1/webserver-v1.min.js"
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


def validate_version_deprecated(config: ConfigType) -> ConfigType:
    if config[CONF_VERSION] == 1:
        _LOGGER.warning(
            "Version 1 of 'web_server' is deprecated and will be removed in "
            "2027.1.0. Please migrate to version 2 (the default) or version 3."
        )
    return config


def validate_auth_type_deprecated(auth: ConfigType) -> ConfigType:
    # Remove before 2027.1.0: the default auth scheme changes from basic to digest.
    if CONF_TYPE not in auth:
        _LOGGER.warning(
            "The 'web_server' 'auth' scheme currently defaults to 'basic', which sends the "
            "password over the network in an easily reversible form. The default will change "
            "to 'digest' in ESPHome 2027.1.0. To keep using basic authentication, set "
            "'type: basic' under 'auth:' explicitly; otherwise set 'type: digest' now to "
            "adopt the more secure scheme."
        )
    return auth


def validate_local(config: ConfigType) -> ConfigType:
    if CONF_LOCAL in config and config[CONF_VERSION] == 1:
        raise cv.Invalid("'local' is not supported in version 1")
    return config


def validate_ota(config: ConfigType) -> ConfigType:
    # The OTA option only accepts False to explicitly disable OTA for web_server
    # IMPORTANT: Setting ota: false ONLY affects the web_server component
    # The captive_portal component will still be able to perform OTA updates
    if CONF_OTA in config and config[CONF_OTA] is not False:
        raise cv.Invalid(
            f"The '{CONF_OTA}' option in 'web_server' only accepts 'false' to disable OTA. "
            f"To enable OTA, please use the new OTA platform structure instead:\n\n"
            f"ota:\n"
            f"  - platform: web_server\n\n"
            f"See https://esphome.io/components/ota for more information."
        )
    return config


# An Origin header is always "scheme://host[:port]" with no path or trailing slash.
_ORIGIN_RE = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*://[^/\s]+$")


def validate_origin(value: str) -> str:
    # "*" is the wildcard that allows any origin.
    if value == "*":
        return value
    value = cv.string_strict(value)
    if not _ORIGIN_RE.match(value):
        raise cv.Invalid(
            f"'{value}' is not a valid origin. An origin must be 'scheme://host[:port]' with no "
            f"path or trailing slash (e.g. 'https://example.com'), or '*' to allow any origin."
        )
    # Browsers send the scheme and host lowercased in the Origin header, so normalize to match.
    return value.lower()


def validate_private_network_access(config: ConfigType) -> ConfigType:
    # PNA preflights are always cross-origin, so they can only be authorized against the
    # allowed_origins list. Enabling PNA without any origins would deny every PNA request.
    if (
        config[CONF_ENABLE_PRIVATE_NETWORK_ACCESS]
        and config.get(CONF_ALLOWED_ORIGINS) is None
    ):
        raise cv.Invalid(
            f"'{CONF_ALLOWED_ORIGINS}' must be set when "
            f"'{CONF_ENABLE_PRIVATE_NETWORK_ACCESS}' is enabled. List each origin that is "
            f"allowed to reach the device (e.g. 'https://example.com'). '*' allows any origin "
            f"but is not recommended.",
            path=[CONF_ENABLE_PRIVATE_NETWORK_ACCESS],
        )
    return config


def validate_sorting_groups(config: ConfigType) -> ConfigType:
    if CONF_SORTING_GROUPS in config and config[CONF_VERSION] != 3:
        raise cv.Invalid(
            f"'{CONF_SORTING_GROUPS}' is only supported in 'web_server' version 3"
        )
    return config


def _validate_no_sorting_component(
    sorting_component: str,
    webserver_version: int,
    config: ConfigType,
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


def _final_validate_sorting(config: ConfigType) -> ConfigType:
    if (webserver_version := config.get(CONF_VERSION)) != 3:
        _validate_no_sorting_component(
            CONF_SORTING_WEIGHT, webserver_version, fv.full_config.get()
        )
        _validate_no_sorting_component(
            CONF_SORTING_GROUP_ID, webserver_version, fv.full_config.get()
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate_sorting


def _consume_web_server_sockets(config: ConfigType) -> ConfigType:
    """Register socket needs for web_server component."""
    from esphome.components import socket

    # Web server needs typically 5 concurrent client connections
    # (browser opens connections for page resources, SSE event stream, and POST
    # requests for entity control which may linger before closing)
    # The listening socket is registered by web_server_base (shared with captive_portal)
    socket.consume_sockets(5, "web_server")(config)
    return config


sorting_group = {
    cv.Required(CONF_ID): cv.declare_id(cg.int_),
    cv.Required(CONF_NAME): cv.string,
    cv.Optional(CONF_SORTING_WEIGHT): cv.float_,
}

WEBSERVER_SORTING_SCHEMA = cv.Schema(
    {
        # The per-entity web_server block is cosmetic dashboard ordering —
        # mark the whole block advanced; the children inherit via the cascade.
        cv.Optional(CONF_WEB_SERVER, visibility=cv.Visibility.ADVANCED): cv.Schema(
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
            cv.Optional(CONF_ENABLE_PRIVATE_NETWORK_ACCESS, default=False): cv.boolean,
            cv.Optional(CONF_ALLOWED_ORIGINS): cv.All(
                cv.ensure_list(validate_origin), cv.Length(min=1)
            ),
            cv.Optional(CONF_AUTH): cv.All(
                cv.Schema(
                    {
                        cv.Required(CONF_USERNAME): cv.All(
                            cv.string_strict, cv.Length(min=1)
                        ),
                        cv.Required(CONF_PASSWORD): cv.sensitive(
                            cv.All(cv.string_strict, cv.Length(min=1))
                        ),
                        cv.Optional(CONF_TYPE): cv.one_of(
                            AUTH_TYPE_BASIC, AUTH_TYPE_DIGEST, lower=True
                        ),
                    }
                ),
                validate_auth_type_deprecated,
            ),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.Optional(CONF_INCLUDE_INTERNAL, default=False): cv.boolean,
            cv.Optional(CONF_OTA): cv.boolean,
            cv.Optional(CONF_LOG, default=True): cv.boolean,
            cv.Optional(CONF_LOCAL): cv.boolean,
            cv.Optional(CONF_COMPRESSION, default="gzip"): cv.one_of("gzip", "br"),
            cv.Optional(CONF_SORTING_GROUPS): cv.ensure_list(sorting_group),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
            PLATFORM_BK72XX,
            PLATFORM_LN882X,
            PLATFORM_RP2,
            PLATFORM_RTL87XX,
        ]
    ),
    default_url,
    validate_version_deprecated,
    validate_local,
    validate_sorting_groups,
    validate_ota,
    validate_private_network_access,
    _consume_web_server_sockets,
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

    cg.add_define("USE_WEBSERVER_SORTING")
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
    uint8_t = f"constexpr uint8_t ESPHOME_WEBSERVER_{resource_name}[{content_encoded_size}] PROGMEM = {{{bytes_as_int}}}"
    size_t = f"constexpr size_t ESPHOME_WEBSERVER_{resource_name}_SIZE = {content_encoded_size}"
    cg.add_global(cg.RawExpression(uint8_t))
    cg.add_global(cg.RawExpression(size_t))


@coroutine_with_priority(CoroPriority.WEB)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    # Track controller registration for StaticVector sizing
    CORE.register_controller()

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
    # OTA is now handled by the web_server OTA platform
    # The CONF_OTA option is kept to allow explicitly disabling OTA for web_server
    # IMPORTANT: This ONLY affects the web_server component, NOT captive_portal
    # Captive portal will still be able to perform OTA updates even when this is set
    if config.get(CONF_OTA) is False:
        cg.add_define("USE_WEBSERVER_OTA_DISABLED")
    cg.add(var.set_expose_log(config[CONF_LOG]))
    if config[CONF_LOG]:
        request_log_listener()  # Request a log listener slot for web server log streaming
    if config[CONF_ENABLE_PRIVATE_NETWORK_ACCESS]:
        cg.add_define("USE_WEBSERVER_PRIVATE_NETWORK_ACCESS")
    if (allowed_origins := config.get(CONF_ALLOWED_ORIGINS)) is not None:
        cg.add_define("USE_WEBSERVER_ALLOWED_ORIGINS")
        cg.add(var.set_allowed_origins(allowed_origins))
    if (auth := config.get(CONF_AUTH)) is not None:
        cg.add_define("USE_WEBSERVER_AUTH")
        # The scheme is fixed at build time so the unused Basic/Digest code path is compiled
        # out. Basic is the current default (the absence of this define); an explicit
        # 'type: digest' opts in early. Default changes to digest in 2027.1.0.
        if auth.get(CONF_TYPE) == AUTH_TYPE_DIGEST:
            cg.add_define("USE_WEBSERVER_AUTH_DIGEST")
        cg.add(paren.set_auth_username(auth[CONF_USERNAME]))
        cg.add(paren.set_auth_password(auth[CONF_PASSWORD]))
    if CONF_CSS_INCLUDE in config:
        cg.add_define("USE_WEBSERVER_CSS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_CSS_INCLUDE])
        with path.open(encoding="utf-8") as css_file:
            add_resource_as_progmem("CSS_INCLUDE", css_file.read())
    if CONF_JS_INCLUDE in config:
        cg.add_define("USE_WEBSERVER_JS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_JS_INCLUDE])
        with path.open(encoding="utf-8") as js_file:
            add_resource_as_progmem("JS_INCLUDE", js_file.read())
    cg.add(var.set_include_internal(config[CONF_INCLUDE_INTERNAL]))
    if CONF_LOCAL in config and config[CONF_LOCAL]:
        cg.add_define("USE_WEBSERVER_LOCAL")
    if config[CONF_COMPRESSION] == "gzip":
        cg.add_define("USE_WEBSERVER_GZIP")

    if (sorting_group_config := config.get(CONF_SORTING_GROUPS)) is not None:
        cg.add_define("USE_WEBSERVER_SORTING")
        add_sorting_groups(var, sorting_group_config)


def FILTER_SOURCE_FILES() -> list[str]:
    """Filter out web_server_v1.cpp when version is not 1."""
    files_to_filter: list[str] = []

    # web_server_v1.cpp is only needed when version is 1
    config = CORE.config.get("web_server", {})
    if config.get(CONF_VERSION, 2) != 1:
        files_to_filter.append("web_server_v1.cpp")

    return files_to_filter
