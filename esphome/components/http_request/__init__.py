import urllib.parse as urlparse

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_TIMEOUT,
    CONF_METHOD,
    CONF_TRIGGER_ID,
    CONF_URL,
    CONF_ESP8266_DISABLE_SSL_SUPPORT,
)
from esphome.core import Lambda, CORE

DEPENDENCIES = ["network"]
AUTO_LOAD = ["json"]

http_request_ns = cg.esphome_ns.namespace("http_request")
HttpRequestComponent = http_request_ns.class_("HttpRequestComponent", cg.Component)
HttpRequestSendAction = http_request_ns.class_(
    "HttpRequestSendAction", automation.Action
)
HttpRequestResponseTrigger = http_request_ns.class_(
    "HttpRequestResponseTrigger", automation.Trigger
)

CONF_HEADERS = "headers"
CONF_USERAGENT = "useragent"
CONF_BODY = "body"
CONF_JSON = "json"
CONF_VERIFY_SSL = "verify_ssl"
CONF_ON_RESPONSE = "on_response"


def validate_url(value):
    value = cv.string(value)
    try:
        parsed = list(urlparse.urlparse(value))
    except Exception as err:
        raise cv.Invalid("Invalid URL") from err

    if not parsed[0] or not parsed[1]:
        raise cv.Invalid("URL must have a URL scheme and host")

    if parsed[0] not in ["http", "https"]:
        raise cv.Invalid("Scheme must be http or https")

    if not parsed[2]:
        parsed[2] = "/"

    return urlparse.urlunparse(parsed)


def validate_secure_url(config):
    url_ = config[CONF_URL]
    if (
        config.get(CONF_VERIFY_SSL)
        and not isinstance(url_, Lambda)
        and url_.lower().startswith("https:")
    ):
        raise cv.Invalid(
            "Currently ESPHome doesn't support SSL verification. "
            "Set 'verify_ssl: false' to make insecure HTTPS requests."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HttpRequestComponent),
            cv.Optional(CONF_USERAGENT, "ESPHome"): cv.string,
            cv.Optional(
                CONF_TIMEOUT, default="5s"
            ): cv.positive_time_period_milliseconds,
            cv.SplitDefault(CONF_ESP8266_DISABLE_SSL_SUPPORT, esp8266=False): cv.All(
                cv.only_on_esp8266, cv.boolean
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(
        esp8266_arduino=cv.Version(2, 5, 1),
        esp32_arduino=cv.Version(0, 0, 0),
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_useragent(config[CONF_USERAGENT]))
    if CORE.is_esp8266 and not config[CONF_ESP8266_DISABLE_SSL_SUPPORT]:
        cg.add_define("USE_HTTP_REQUEST_ESP8266_HTTPS")

    if CORE.is_esp32:
        cg.add_library("WiFiClientSecure", None)
        cg.add_library("HTTPClient", None)
    if CORE.is_esp8266:
        cg.add_library("ESP8266HTTPClient", None)

    await cg.register_component(var, config)


HTTP_REQUEST_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(HttpRequestComponent),
        cv.Required(CONF_URL): cv.templatable(validate_url),
        cv.Optional(CONF_HEADERS): cv.All(
            cv.Schema({cv.string: cv.templatable(cv.string)})
        ),
        cv.Optional(CONF_VERIFY_SSL, default=True): cv.boolean,
        cv.Optional(CONF_ON_RESPONSE): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(HttpRequestResponseTrigger)}
        ),
    }
).add_extra(validate_secure_url)
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
    if CONF_BODY in config:
        template_ = await cg.templatable(config[CONF_BODY], args, cg.std_string)
        cg.add(var.set_body(template_))
    if CONF_JSON in config:
        json_ = config[CONF_JSON]
        if isinstance(json_, Lambda):
            args_ = args + [(cg.JsonObjectRef, "root")]
            lambda_ = await cg.process_lambda(json_, args_, return_type=cg.void)
            cg.add(var.set_json(lambda_))
        else:
            for key in json_:
                template_ = await cg.templatable(json_[key], args, cg.std_string)
                cg.add(var.add_json(key, template_))
    for key in config.get(CONF_HEADERS, []):
        template_ = await cg.templatable(
            config[CONF_HEADERS][key], args, cg.const_char_ptr
        )
        cg.add(var.add_header(key, template_))

    for conf in config.get(CONF_ON_RESPONSE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_response_trigger(trigger))
        await automation.build_automation(trigger, [(int, "status_code")], conf)

    return var
