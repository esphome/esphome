from esphome import automation
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import (
    CONF_CAPTURE_RESPONSE,
    CONF_CERTIFICATE_AUTHORITY,
    CONF_CLIENT_CERTIFICATE,
    CONF_CLIENT_CERTIFICATE_KEY,
    CONF_ID,
    CONF_METHOD,
    CONF_ON_RESPONSE,
    CONF_PAYLOAD,
    CONF_URL,
)
from esphome.core import Lambda

from .const import (
    CONF_ACK_TIMEOUT,
    CONF_JSON,
    CONF_MAX_BLOCK_SIZE,
    CONF_MAX_RESPONSE_BUFFER_SIZE,
    CONF_MAX_RETRANSMIT,
    CONF_MEDIA_TYPE,
    CONF_OBSERVE,
    CONF_OSCORE_CONF,
    CONF_PSK_IDENTITY,
    CONF_PSK_KEY,
    CONF_REQUEST_NAME,
    CONF_REQUEST_TIMEOUT,
    CONF_RESPONSE_TIMEOUT,
)


def validate_url(value):
    value = cv.url(value)
    if value.startswith("coap") or value.startswith("coaps"):
        return value
    raise cv.Invalid("URL must start with 'coap[s][+tcp|+ws]'")


CONF_COAP_CLIENT_ID = "coap_client_id"
CODEOWNERS = ["@rwrozelle"]

DEPENDENCIES = ["network"]
AUTO_LOAD = ["json", "watchdog"]

coap_client_component_ns = cg.esphome_ns.namespace("coap_client")
CoapClientComponent = coap_client_component_ns.class_(
    "CoapClientComponent", cg.Component
)
CoapResponseStatistics = coap_client_component_ns.class_("CoapResponseStatistics")

CoapClientSendAction = coap_client_component_ns.class_(
    "CoapClientSendAction", automation.Action
)
CoapClientRemoveAction = coap_client_component_ns.class_(
    "CoapClientRemoveAction", automation.Action
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CoapClientComponent),
        cv.Optional(CONF_MAX_BLOCK_SIZE, default="512B"): cv.validate_bytes,
        cv.Optional(
            CONF_REQUEST_TIMEOUT, default="2sec"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_ACK_TIMEOUT, default="2sec"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_RETRANSMIT, default=4): cv.uint8_t,
        cv.Optional(CONF_OSCORE_CONF): cv.string,
        cv.Optional(CONF_PSK_IDENTITY): cv.string,
        cv.Optional(CONF_PSK_KEY): cv.string,
        cv.Optional(CONF_CERTIFICATE_AUTHORITY): cv.string,
        cv.Optional(CONF_CLIENT_CERTIFICATE): cv.string,
        cv.Optional(CONF_CLIENT_CERTIFICATE_KEY): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_define("USE_COAP_CLIENT")
    add_idf_component(name="espressif/coap", ref="4.3.5~3")
    add_idf_sdkconfig_option("CONFIG_COAP_CLIENT_SUPPORT", True)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if (max_block_size := config.get(CONF_MAX_BLOCK_SIZE)) is not None:
        cg.add(var.set_max_block_size(max_block_size))
    if (request_timeout := config.get(CONF_REQUEST_TIMEOUT)) is not None:
        cg.add(var.set_request_timeout(request_timeout))
    if (ack_timeout := config.get(CONF_ACK_TIMEOUT)) is not None:
        cg.add(var.set_ack_timeout(ack_timeout))
    if (max_retransmit := config.get(CONF_MAX_RETRANSMIT)) is not None:
        cg.add(var.set_max_retransmit(max_retransmit))
    if (oscore_conf := config.get(CONF_OSCORE_CONF)) is not None:
        add_idf_sdkconfig_option("CONFIG_COAP_OSCORE_SUPPORT", True)
        cg.add(var.set_oscore_conf(oscore_conf))
    if (psk_identity := config.get(CONF_PSK_IDENTITY)) is not None:
        add_idf_sdkconfig_option("CONFIG_COAP_MBEDTLS_PSK", True)
        cg.add(var.set_psk_identity(psk_identity))
    if (psk_key := config.get(CONF_PSK_KEY)) is not None:
        cg.add(var.set_psk_key(psk_key))
    if (ca_pem := config.get(CONF_CERTIFICATE_AUTHORITY)) is not None:
        add_idf_sdkconfig_option("CONFIG_COAP_MBEDTLS_PKI", True)
        cg.add(var.set_psk_key(ca_pem))
    if (client_crt := config.get(CONF_CLIENT_CERTIFICATE)) is not None:
        cg.add(var.set_client_crt(client_crt))
    if (client_key := config.get(CONF_CLIENT_CERTIFICATE_KEY)) is not None:
        cg.add(var.set_client_key(client_key))


CONF_MEDIA_TYPES = {
    "text/plain": "TEXT_PLAIN",
    "application/json": "APPLICATION_JSON",
    "application/link_format": "APPLICATION_LINK_FORMAT",
    "application/xml": "APPLICATION_XML",
    "application/octet_stream": "APPLICATION_OCTET_STREAM",
    "application/rdf_xml": "APPLICATION_RDF_XML",
    "application/exi": "APPLICATION_EXI",
    "application/cbor": "APPLICATION_CBOR",
    "application/cwt": "APPLICATION_CWT",
}

COAP_CLIENT_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(CoapClientComponent),
        cv.Required(CONF_URL): cv.templatable(validate_url),
        cv.Optional(CONF_REQUEST_NAME): cv.string,
        cv.Optional(CONF_CAPTURE_RESPONSE, default=False): cv.boolean,
        cv.Optional(CONF_MAX_RESPONSE_BUFFER_SIZE, default="1kB"): cv.validate_bytes,
        cv.Optional(
            CONF_RESPONSE_TIMEOUT, default="4sec"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_OBSERVE, default="False"): cv.boolean,
        cv.Optional(CONF_ON_RESPONSE): automation.validate_automation(single=True),
    }
)
COAP_CLIENT_GET_ACTION_SCHEMA = automation.maybe_conf(
    CONF_URL,
    COAP_CLIENT_ACTION_SCHEMA.extend(
        {
            cv.Optional(CONF_METHOD, default="GET"): cv.one_of("GET", upper=True),
        }
    ),
)
COAP_CLIENT_POST_ACTION_SCHEMA = automation.maybe_conf(
    CONF_URL,
    COAP_CLIENT_ACTION_SCHEMA.extend(
        {
            cv.Optional(CONF_METHOD, default="POST"): cv.one_of("POST", upper=True),
            cv.Optional(CONF_MEDIA_TYPE, default="text/plain"): cv.one_of(
                *CONF_MEDIA_TYPES.keys(), lower=True
            ),
            cv.Exclusive(CONF_PAYLOAD, "payload"): cv.templatable(cv.string),
            cv.Exclusive(CONF_JSON, "payload"): cv.Any(
                cv.lambda_,
                cv.All(cv.Schema({cv.string: cv.templatable(cv.string_strict)})),
            ),
        }
    ),
)
COAP_CLIENT_SEND_ACTION_SCHEMA = automation.maybe_conf(
    CONF_URL,
    COAP_CLIENT_ACTION_SCHEMA.extend(
        {
            cv.Required(CONF_METHOD): cv.one_of(
                "GET", "POST", "PUT", "DELETE", "PATCH", upper=True
            ),
            cv.Optional(CONF_MEDIA_TYPE, default="text/plain"): cv.one_of(
                *CONF_MEDIA_TYPES, lower=True
            ),
            cv.Exclusive(CONF_PAYLOAD, "payload"): cv.templatable(cv.string),
            cv.Exclusive(CONF_JSON, "payload"): cv.Any(
                cv.lambda_,
                cv.All(cv.Schema({cv.string: cv.templatable(cv.string_strict)})),
            ),
        }
    ),
)
COAP_CLIENT_REMOVE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(CoapClientComponent),
        cv.Optional(CONF_REQUEST_NAME): cv.string,
    }
)


@automation.register_action(
    "coap_client.get", CoapClientSendAction, COAP_CLIENT_GET_ACTION_SCHEMA
)
@automation.register_action(
    "coap_client.post", CoapClientSendAction, COAP_CLIENT_POST_ACTION_SCHEMA
)
@automation.register_action(
    "coap_client.send", CoapClientSendAction, COAP_CLIENT_SEND_ACTION_SCHEMA
)
async def coap_client_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_URL], args, cg.std_string)
    cg.add(var.set_url(template_))
    if (name := config.get(CONF_REQUEST_NAME)) is not None:
        cg.add(var.set_request_name(name))
    cg.add(var.set_method(config[CONF_METHOD]))
    if (media_type := config.get(CONF_MEDIA_TYPE)) is not None:
        if CONF_JSON in config:
            cg.add(var.set_media_type("APPLICATION_JSON"))
        else:
            media_type_code = CONF_MEDIA_TYPES[media_type]
            cg.add(var.set_media_type(media_type_code))
    if CONF_PAYLOAD in config:
        template_ = await cg.templatable(config[CONF_PAYLOAD], args, cg.std_string)
        cg.add(var.set_payload(template_))
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

    if capture_response := config.get(CONF_CAPTURE_RESPONSE):
        cg.add(var.set_capture_response(capture_response))
    if (response_timeout := config.get(CONF_RESPONSE_TIMEOUT)) is not None:
        cg.add(var.set_response_timeout(response_timeout))
    if observe := config.get(CONF_OBSERVE):
        cg.add(var.set_observe(observe))
    if (max_buffer := config.get(CONF_MAX_RESPONSE_BUFFER_SIZE)) is not None:
        cg.add(var.set_max_response_buffer_size(max_buffer))
    if response_conf := config.get(CONF_ON_RESPONSE):
        await automation.build_automation(
            var.get_success_trigger(),
            [
                (cg.std_shared_ptr.template(CoapResponseStatistics), "response"),
                (cg.std_string_ref, "request_name"),
                (cg.std_string_ref, "payload"),
                *args,
            ],
            response_conf,
        )

    return var


@automation.register_action(
    "coap_client.remove", CoapClientRemoveAction, COAP_CLIENT_REMOVE_ACTION_SCHEMA
)
async def coap_client_remove_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if (name := config.get(CONF_REQUEST_NAME)) is not None:
        cg.add(var.set_request_name(name))

    return var
