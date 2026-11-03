import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.esp32 import (
    include_builtin_idf_component,
    request_tls,
    require_mbedtls_tls_server,
    set_idf_sdkconfig_default,
)
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import CONF_CERTIFICATE, CONF_KEY, CONF_PORT
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@dentra"]
DOMAIN = "web_server_idf"

CONF_SSL = "ssl"

# Components that use the shared server in ways TLS does not support yet:
# web_server streams events from the main loop straight to the socket, and
# phones check for a captive portal over plain HTTP.
SSL_INCOMPATIBLE_COMPONENTS = ("web_server", "captive_portal")


def _pem(marker: str):
    def validator(value: object) -> str:
        value = cv.string_strict(value)
        if marker not in value:
            raise cv.Invalid(f"Expected a PEM block containing '{marker}'")
        return value

    return validator


SSL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(web_server_base.CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
        cv.Optional(CONF_PORT, default=443): cv.port,
        cv.Required(CONF_CERTIFICATE): _pem("-----BEGIN CERTIFICATE-----"),
        # Every PEM private key form (PKCS#8, EC, RSA, encrypted) ends its header this way.
        cv.Required(CONF_KEY): cv.sensitive(_pem("PRIVATE KEY-----")),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_SSL): SSL_SCHEMA,
        }
    ),
    cv.only_on_esp32,
)


def _final_validate(config: ConfigType) -> None:
    if CONF_SSL not in config:
        return
    full_config = fv.full_config.get()
    for component in SSL_INCOMPATIBLE_COMPONENTS:
        if component in full_config:
            raise cv.Invalid(
                f"'{CONF_SSL}' can't be used together with '{component}' yet"
            )


FINAL_VALIDATE_SCHEMA = _final_validate


def enable_ssl() -> None:
    """Compile in HTTPS support for the shared web server.

    Components that supply their own certificate at run time call this from
    their to_code(), then call WebServerBase::set_ssl_credentials().
    """
    cg.add_define("USE_WEBSERVER_SSL")
    request_tls()
    # ESPHome builds mbedTLS client-only unless a component accepts TLS sessions.
    require_mbedtls_tls_server()
    include_builtin_idf_component("esp_https_server")
    set_idf_sdkconfig_default("CONFIG_ESP_HTTPS_SERVER_ENABLE", True)


async def to_code(config: ConfigType) -> None:
    # Increase the maximum supported size of headers section in HTTP request packet to be processed by the server
    set_idf_sdkconfig_default("CONFIG_HTTPD_MAX_REQ_HDR_LEN", 1024)
    # Re-enable ESP-IDF's HTTP server (excluded by default to save compile time).
    # Basic auth uses mbedtls_base64_encode directly, so no TLS stack is needed.
    include_builtin_idf_component("esp_http_server")

    if (ssl := config.get(CONF_SSL)) is not None:
        enable_ssl()
        base = await cg.get_variable(ssl[web_server_base.CONF_WEB_SERVER_BASE_ID])
        cg.add(base.set_port(ssl[CONF_PORT]))
        cg.add(base.set_ssl_credentials(ssl[CONF_CERTIFICATE], ssl[CONF_KEY]))


# multipart.cpp is fully #ifdef'd on USE_WEBSERVER_OTA (set by the
# web_server OTA platform); skip it when OTA uploads are not configured.
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {"multipart.cpp": "USE_WEBSERVER_OTA"}
)
