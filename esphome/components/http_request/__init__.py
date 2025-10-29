from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.const import CONF_REQUEST_HEADERS
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_CAPTURE_RESPONSE,
    CONF_ESP8266_DISABLE_SSL_SUPPORT,
    CONF_ID,
    CONF_METHOD,
    CONF_ON_ERROR,
    CONF_ON_RESPONSE,
    CONF_TIMEOUT,
    CONF_URL,
    CONF_WATCHDOG_TIMEOUT,
    PLATFORM_HOST,
    PlatformFramework,
    __version__,
)
from esphome.core import CORE, Lambda
from esphome.helpers import IS_MACOS

DEPENDENCIES = ["network"]
AUTO_LOAD = ["json", "watchdog"]

http_request_ns = cg.esphome_ns.namespace("http_request")
HttpRequestComponent = http_request_ns.class_("HttpRequestComponent", cg.Component)
HttpRequestArduino = http_request_ns.class_("HttpRequestArduino", HttpRequestComponent)
HttpRequestIDF = http_request_ns.class_("HttpRequestIDF", HttpRequestComponent)
HttpRequestHost = http_request_ns.class_("HttpRequestHost", HttpRequestComponent)

HttpContainer = http_request_ns.class_("HttpContainer")

HttpRequestSendAction = http_request_ns.class_(
    "HttpRequestSendAction", automation.Action
)
HttpRequestResponseTrigger = http_request_ns.class_(
    "HttpRequestResponseTrigger",
    automation.Trigger.template(
        cg.std_shared_ptr.template(HttpContainer), cg.std_string
    ),
)

CONF_HTTP_REQUEST_ID = "http_request_id"

CONF_USERAGENT = "useragent"
CONF_VERIFY_SSL = "verify_ssl"
CONF_FOLLOW_REDIRECTS = "follow_redirects"
CONF_REDIRECT_LIMIT = "redirect_limit"
CONF_BUFFER_SIZE_RX = "buffer_size_rx"
CONF_BUFFER_SIZE_TX = "buffer_size_tx"
CONF_CA_CERTIFICATE_PATH = "ca_certificate_path"

CONF_MAX_RESPONSE_BUFFER_SIZE = "max_response_buffer_size"
CONF_HEADERS = "headers"
CONF_COLLECT_HEADERS = "collect_headers"
CONF_BODY = "body"
CONF_JSON = "json"


def validate_url(value):
    value = cv.url(value)
    if value.startswith("http://") or value.startswith("https://"):
        return value
    raise cv.Invalid("URL must start with 'http://' or 'https://'")


def validate_ssl_verification(config):
    error_message = ""

    if CORE.is_esp32 and not CORE.using_esp_idf and config[CONF_VERIFY_SSL]:
        error_message = "ESPHome supports certificate verification only via ESP-IDF"

    if CORE.is_rp2040 and config[CONF_VERIFY_SSL]:
        error_message = "ESPHome does not support certificate verification on RP2040"

    if (
        CORE.is_esp8266
        and not config[CONF_ESP8266_DISABLE_SSL_SUPPORT]
        and config[CONF_VERIFY_SSL]
    ):
        error_message = "ESPHome does not support certificate verification on ESP8266"

    if len(error_message) > 0:
        raise cv.Invalid(
            f"{error_message}. Set '{CONF_VERIFY_SSL}: false' to skip certificate validation and allow less secure HTTPS connections."
        )

    return config


def _declare_request_class(value):
    if CORE.is_host:
        return cv.declare_id(HttpRequestHost)(value)
    if CORE.using_esp_idf:
        return cv.declare_id(HttpRequestIDF)(value)
    if CORE.is_esp8266 or CORE.is_esp32 or CORE.is_rp2040:
        return cv.declare_id(HttpRequestArduino)(value)
    return NotImplementedError


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): _declare_request_class,
            cv.Optional(
                CONF_USERAGENT, f"ESPHome/{__version__} (https://esphome.io)"
            ): cv.string,
            cv.Optional(CONF_FOLLOW_REDIRECTS, True): cv.boolean,
            cv.Optional(CONF_REDIRECT_LIMIT, 3): cv.int_,
            cv.Optional(
                CONF_TIMEOUT, default="4.5s"
            ): cv.positive_time_period_milliseconds,
            cv.SplitDefault(CONF_ESP8266_DISABLE_SSL_SUPPORT, esp8266=False): cv.All(
                cv.only_on_esp8266, cv.boolean
            ),
            cv.Optional(CONF_VERIFY_SSL, default=True): cv.boolean,
            cv.Optional(CONF_WATCHDOG_TIMEOUT): cv.All(
                cv.Any(cv.only_on_esp32, cv.only_on_rp2040),
                cv.positive_not_null_time_period,
                cv.positive_time_period_milliseconds,
            ),
            cv.SplitDefault(CONF_BUFFER_SIZE_RX, esp32_idf=512): cv.All(
                cv.uint16_t, cv.only_with_esp_idf
            ),
            cv.SplitDefault(CONF_BUFFER_SIZE_TX, esp32_idf=512): cv.All(
                cv.uint16_t, cv.only_with_esp_idf
            ),
            cv.Optional(CONF_CA_CERTIFICATE_PATH): cv.All(
                cv.file_,
                cv.only_on(PLATFORM_HOST),
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(
        esp8266_arduino=cv.Version(2, 5, 1),
        esp32_arduino=cv.Version(0, 0, 0),
        esp_idf=cv.Version(0, 0, 0),
        rp2040_arduino=cv.Version(0, 0, 0),
        host=cv.Version(0, 0, 0),
    ),
    validate_ssl_verification,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_useragent(config[CONF_USERAGENT]))
    cg.add(var.set_follow_redirects(config[CONF_FOLLOW_REDIRECTS]))
    cg.add(var.set_redirect_limit(config[CONF_REDIRECT_LIMIT]))

    if CORE.is_esp8266 and not config[CONF_ESP8266_DISABLE_SSL_SUPPORT]:
        cg.add_define("USE_HTTP_REQUEST_ESP8266_HTTPS")

    if timeout_ms := config.get(CONF_WATCHDOG_TIMEOUT):
        cg.add(var.set_watchdog_timeout(timeout_ms))

    if CORE.is_esp32:
        if CORE.using_esp_idf:
            cg.add(var.set_buffer_size_rx(config[CONF_BUFFER_SIZE_RX]))
            cg.add(var.set_buffer_size_tx(config[CONF_BUFFER_SIZE_TX]))

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
            cg.add_library("NetworkClientSecure", None)
            cg.add_library("HTTPClient", None)
    if CORE.is_esp8266:
        cg.add_library("ESP8266HTTPClient", None)
    if CORE.is_rp2040 and CORE.using_arduino:
        cg.add_library("HTTPClient", None)
    if CORE.is_host:
        if IS_MACOS:
            cg.add_build_flag("-I/opt/homebrew/opt/openssl/include")
            cg.add_build_flag("-L/opt/homebrew/opt/openssl/lib")
            cg.add_build_flag("-lssl")
            cg.add_build_flag("-lcrypto")
            cg.add_build_flag("-Wl,-framework,CoreFoundation")
            cg.add_build_flag("-Wl,-framework,Security")
            cg.add_define("CPPHTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN")
            cg.add_define("CPPHTTPLIB_OPENSSL_SUPPORT")
        elif path := config.get(CONF_CA_CERTIFICATE_PATH):
            cg.add_define("CPPHTTPLIB_OPENSSL_SUPPORT")
            cg.add(var.set_ca_path(str(path)))
            cg.add_build_flag("-lssl")
            cg.add_build_flag("-lcrypto")

    await cg.register_component(var, config)


HTTP_REQUEST_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(HttpRequestComponent),
        cv.Required(CONF_URL): cv.templatable(validate_url),
        cv.Optional(CONF_HEADERS): cv.invalid(
            "The 'headers' options has been renamed to 'request_headers'"
        ),
        cv.Optional(CONF_REQUEST_HEADERS): cv.All(
            cv.Schema({cv.string: cv.templatable(cv.string)})
        ),
        cv.Optional(CONF_COLLECT_HEADERS): cv.ensure_list(cv.string),
        cv.Optional(CONF_VERIFY_SSL): cv.invalid(
            f"{CONF_VERIFY_SSL} has moved to the base component configuration."
        ),
        cv.Optional(CONF_CAPTURE_RESPONSE, default=False): cv.boolean,
        cv.Optional(CONF_ON_RESPONSE): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(single=True),
        cv.Optional(CONF_MAX_RESPONSE_BUFFER_SIZE, default="1kB"): cv.validate_bytes,
    }
)
HTTP_REQUEST_GET_ACTION_SCHEMA = automation.maybe_conf(
    CONF_URL,
    HTTP_REQUEST_ACTION_SCHEMA.extend(
        {
            cv.Optional(CONF_METHOD, default="GET"): cv.one_of("GET", upper=True),
        }
    ),
)
HTTP_REQUEST_POST_ACTION_SCHEMA = automation.maybe_conf(
    CONF_URL,
    HTTP_REQUEST_ACTION_SCHEMA.extend(
        {
            cv.Optional(CONF_METHOD, default="POST"): cv.one_of("POST", upper=True),
            cv.Exclusive(CONF_BODY, "body"): cv.templatable(cv.string),
            cv.Exclusive(CONF_JSON, "body"): cv.Any(
                cv.lambda_,
                cv.All(cv.Schema({cv.string: cv.templatable(cv.string_strict)})),
            ),
        }
    ),
)
HTTP_REQUEST_SEND_ACTION_SCHEMA = HTTP_REQUEST_ACTION_SCHEMA.extend(
    {
        cv.Required(CONF_METHOD): cv.one_of(
            "GET", "POST", "PUT", "DELETE", "PATCH", upper=True
        ),
        cv.Exclusive(CONF_BODY, "body"): cv.templatable(cv.string),
        cv.Exclusive(CONF_JSON, "body"): cv.Any(
            cv.lambda_,
            cv.All(cv.Schema({cv.string: cv.templatable(cv.string_strict)})),
        ),
    }
)


@automation.register_action(
    "http_request.get", HttpRequestSendAction, HTTP_REQUEST_GET_ACTION_SCHEMA
)
@automation.register_action(
    "http_request.post", HttpRequestSendAction, HTTP_REQUEST_POST_ACTION_SCHEMA
)
@automation.register_action(
    "http_request.send", HttpRequestSendAction, HTTP_REQUEST_SEND_ACTION_SCHEMA
)
async def http_request_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_URL], args, cg.std_string)
    cg.add(var.set_url(template_))
    cg.add(var.set_method(config[CONF_METHOD]))

    capture_response = config[CONF_CAPTURE_RESPONSE]
    if capture_response:
        cg.add(var.set_capture_response(capture_response))
        cg.add_define("USE_HTTP_REQUEST_RESPONSE")

    cg.add(var.set_max_response_buffer_size(config[CONF_MAX_RESPONSE_BUFFER_SIZE]))

    if CONF_BODY in config:
        template_ = await cg.templatable(config[CONF_BODY], args, cg.std_string)
        cg.add(var.set_body(template_))
    if CONF_JSON in config:
        json_ = config[CONF_JSON]
        if isinstance(json_, Lambda):
            args_ = args + [(cg.JsonObject, "root")]
            lambda_ = await cg.process_lambda(json_, args_, return_type=cg.void)
            cg.add(var.set_json(lambda_))
        else:
            for key in json_:
                template_ = await cg.templatable(json_[key], args, cg.std_string)
                cg.add(var.add_json(key, template_))
    for key, value in config.get(CONF_REQUEST_HEADERS, {}).items():
        template_ = await cg.templatable(value, args, cg.const_char_ptr)
        cg.add(var.add_request_header(key, template_))

    for value in config.get(CONF_COLLECT_HEADERS, []):
        cg.add(var.add_collect_header(value))

    if response_conf := config.get(CONF_ON_RESPONSE):
        if capture_response:
            await automation.build_automation(
                var.get_success_trigger_with_response(),
                [
                    (cg.std_shared_ptr.template(HttpContainer), "response"),
                    (cg.std_string_ref, "body"),
                    *args,
                ],
                response_conf,
            )
        else:
            await automation.build_automation(
                var.get_success_trigger(),
                [(cg.std_shared_ptr.template(HttpContainer), "response"), *args],
                response_conf,
            )

    if error_conf := config.get(CONF_ON_ERROR):
        await automation.build_automation(var.get_error_trigger(), args, error_conf)

    return var


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "http_request_host.cpp": {PlatformFramework.HOST_NATIVE},
        "http_request_arduino.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP8266_ARDUINO,
            PlatformFramework.RP2040_ARDUINO,
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        "http_request_idf.cpp": {PlatformFramework.ESP32_IDF},
    }
)
