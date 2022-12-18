from esphome.const import CONF_ID, CONF_PORT, CONF_NAME, CONF_PROTOCOL, CONF_SERVICES
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]

mdns_ns = cg.esphome_ns.namespace("mdns")
MDNSComponent = mdns_ns.class_("MDNSComponent", cg.Component)
MDNSTXTRecord = mdns_ns.struct("MDNSTXTRecord")
MDNSService = mdns_ns.struct("MDNSService")


def _remove_id_if_disabled(value):
    value = value.copy()
    if value[CONF_DISABLED]:
        value.pop(CONF_ID)
    return value


CONF_TXT = "txt"

SERVICE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.string,
        cv.Required(CONF_PROTOCOL): cv.string,
        cv.Optional(CONF_PORT, default=0): cv.Any(0, cv.port),
        cv.Optional(CONF_TXT, default={}): {cv.string: cv.string},
    }
)

CONF_DISABLED = "disabled"
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MDNSComponent),
            cv.Optional(CONF_DISABLED, default=False): cv.boolean,
            cv.Optional(CONF_SERVICES, default=[]): cv.ensure_list(SERVICE_SCHEMA),
        }
    ),
    _remove_id_if_disabled,
)


@coroutine_with_priority(55.0)
async def to_code(config):
    if CORE.using_arduino:
        if CORE.is_esp32:
            cg.add_library("ESPmDNS", None)
        elif CORE.is_esp8266:
            cg.add_library("ESP8266mDNS", None)
        elif CORE.is_rp2040:
            cg.add_library("LEAmDNS", None)

    if config[CONF_DISABLED]:
        return

    cg.add_define("USE_MDNS")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    for service in config[CONF_SERVICES]:
        txt = [
            cg.StructInitializer(
                MDNSTXTRecord,
                ("key", txt_key),
                ("value", txt_value),
            )
            for txt_key, txt_value in service[CONF_TXT].items()
        ]

        exp = cg.StructInitializer(
            MDNSService,
            ("service_type", service[CONF_NAME]),
            ("proto", service[CONF_PROTOCOL]),
            ("port", service[CONF_PORT]),
            ("txt_records", txt),
        )
        cg.add(var.add_extra_service(exp))
