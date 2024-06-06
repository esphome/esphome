import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ESP8266_DISABLE_SSL_SUPPORT,
    CONF_ID,
    CONF_PASSWORD,
    CONF_TIMEOUT,
    CONF_URL,
    CONF_USERNAME,
)
from esphome.components import esp32
from esphome.components.ota import BASE_OTA_SCHEMA, ota_to_code, OTAComponent
from esphome.core import CORE, coroutine_with_priority
from .. import http_request_ns

CODEOWNERS = ["@oarcher"]

AUTO_LOAD = ["md5"]
DEPENDENCIES = ["network"]

CONF_MD5 = "md5"
CONF_MD5_URL = "md5_url"
CONF_VERIFY_SSL = "verify_ssl"
CONF_WATCHDOG_TIMEOUT = "watchdog_timeout"

OtaHttpRequestComponent = http_request_ns.class_(
    "OtaHttpRequestComponent", OTAComponent
)
OtaHttpRequestComponentArduino = http_request_ns.class_(
    "OtaHttpRequestComponentArduino", OtaHttpRequestComponent
)
OtaHttpRequestComponentIDF = http_request_ns.class_(
    "OtaHttpRequestComponentIDF", OtaHttpRequestComponent
)
OtaHttpRequestComponentFlashAction = http_request_ns.class_(
    "OtaHttpRequestComponentFlashAction", automation.Action
)


def validate_ssl_verification(config):
    error_message = ""

    if CORE.is_esp32:
        if not CORE.using_esp_idf and config[CONF_VERIFY_SSL]:
            error_message = "ESPHome supports certificate verification only via ESP-IDF"

    if CORE.is_rp2040 and config[CONF_VERIFY_SSL]:
        error_message = "ESPHome does not support certificate verification in Arduino"

    if (
        CORE.is_esp8266
        and not config[CONF_ESP8266_DISABLE_SSL_SUPPORT]
        and config[CONF_VERIFY_SSL]
    ):
        error_message = "ESPHome does not support certificate verification in Arduino"

    if len(error_message) > 0:
        raise cv.Invalid(
            f"{error_message}. Set '{CONF_VERIFY_SSL}: false' to skip certificate validation and allow less secure HTTPS connections."
        )

    return config


def _declare_request_class(value):
    if CORE.using_esp_idf:
        return cv.declare_id(OtaHttpRequestComponentIDF)(value)

    if CORE.is_esp8266 or CORE.is_esp32 or CORE.is_rp2040:
        return cv.declare_id(OtaHttpRequestComponentArduino)(value)
    return NotImplementedError


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): _declare_request_class,
            cv.SplitDefault(CONF_ESP8266_DISABLE_SSL_SUPPORT, esp8266=False): cv.All(
                cv.only_on_esp8266, cv.boolean
            ),
            cv.Optional(CONF_VERIFY_SSL, default=True): cv.boolean,
            cv.Optional(
                CONF_TIMEOUT, default="5min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_WATCHDOG_TIMEOUT): cv.All(
                cv.Any(cv.only_on_esp32, cv.only_on_rp2040),
                cv.positive_not_null_time_period,
                cv.positive_time_period_milliseconds,
            ),
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(
        esp8266_arduino=cv.Version(2, 5, 1),
        esp32_arduino=cv.Version(0, 0, 0),
        esp_idf=cv.Version(0, 0, 0),
        rp2040_arduino=cv.Version(0, 0, 0),
    ),
    validate_ssl_verification,
)


@coroutine_with_priority(52.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)

    cg.add(var.set_timeout(config[CONF_TIMEOUT]))

    if timeout_ms := config.get(CONF_WATCHDOG_TIMEOUT):
        cg.add_define(
            "USE_HTTP_REQUEST_OTA_WATCHDOG_TIMEOUT",
            timeout_ms,
        )

    if CORE.is_esp8266 and not config[CONF_ESP8266_DISABLE_SSL_SUPPORT]:
        cg.add_define("USE_HTTP_REQUEST_ESP8266_HTTPS")

    if CORE.is_esp32:
        if CORE.using_esp_idf:
            esp32.add_idf_sdkconfig_option(
                "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE",
                config.get(CONF_VERIFY_SSL),
            )
            esp32.add_idf_sdkconfig_option(
                "CONFIG_ESP_TLS_INSECURE",
                not config.get(CONF_VERIFY_SSL),
            )
            esp32.add_idf_sdkconfig_option(
                "CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY",
                not config.get(CONF_VERIFY_SSL),
            )
        else:
            cg.add_library("WiFiClientSecure", None)
            cg.add_library("HTTPClient", None)
    if CORE.is_esp8266:
        cg.add_library("ESP8266HTTPClient", None)
    if CORE.is_rp2040 and CORE.using_arduino:
        cg.add_library("HTTPClient", None)

    await cg.register_component(var, config)


OTA_HTTP_REQUEST_FLASH_ACTION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(OtaHttpRequestComponent),
            cv.Optional(CONF_MD5_URL): cv.templatable(cv.url),
            cv.Optional(CONF_MD5): cv.templatable(cv.string),
            cv.Optional(CONF_PASSWORD): cv.templatable(cv.string),
            cv.Optional(CONF_USERNAME): cv.templatable(cv.string),
            cv.Required(CONF_URL): cv.templatable(cv.url),
        }
    ),
    cv.has_exactly_one_key(CONF_MD5, CONF_MD5_URL),
)


@automation.register_action(
    "ota_http_request.flash",
    OtaHttpRequestComponentFlashAction,
    OTA_HTTP_REQUEST_FLASH_ACTION_SCHEMA,
)
async def ota_http_request_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    if md5_url := config.get(CONF_MD5_URL):
        template_ = await cg.templatable(md5_url, args, cg.std_string)
        cg.add(var.set_md5_url(template_))

    if md5_str := config.get(CONF_MD5):
        template_ = await cg.templatable(md5_str, args, cg.std_string)
        cg.add(var.set_md5(template_))

    if password_str := config.get(CONF_PASSWORD):
        template_ = await cg.templatable(password_str, args, cg.std_string)
        cg.add(var.set_password(template_))

    if username_str := config.get(CONF_USERNAME):
        template_ = await cg.templatable(username_str, args, cg.std_string)
        cg.add(var.set_username(template_))

    template_ = await cg.templatable(config[CONF_URL], args, cg.std_string)
    cg.add(var.set_url(template_))

    return var
