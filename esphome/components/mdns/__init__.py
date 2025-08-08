import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISABLED,
    CONF_ID,
    CONF_PORT,
    CONF_PROTOCOL,
    CONF_SERVICE,
    CONF_SERVICES,
    PlatformFramework,
)
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
        cv.Required(CONF_SERVICE): cv.string,
        cv.Required(CONF_PROTOCOL): cv.string,
        cv.Optional(CONF_PORT, default=0): cv.templatable(cv.Any(0, cv.port)),
        cv.Optional(CONF_TXT, default={}): {cv.string: cv.templatable(cv.string)},
    }
)

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


def mdns_txt_record(key: str, value: str):
    return cg.StructInitializer(
        MDNSTXTRecord,
        ("key", key),
        ("value", value),
    )


def mdns_service(
    service: str, proto: str, port: int, txt_records: list[dict[str, str]]
):
    return cg.StructInitializer(
        MDNSService,
        ("service_type", service),
        ("proto", proto),
        ("port", port),
        ("txt_records", txt_records),
    )


@coroutine_with_priority(55.0)
async def to_code(config):
    if config[CONF_DISABLED] is True:
        return

    if CORE.using_arduino:
        if CORE.is_esp32:
            cg.add_library("ESPmDNS", None)
        elif CORE.is_esp8266:
            cg.add_library("ESP8266mDNS", None)
        elif CORE.is_rp2040:
            cg.add_library("LEAmDNS", None)

    if CORE.using_esp_idf:
        add_idf_component(name="espressif/mdns", ref="1.8.2")

    cg.add_define("USE_MDNS")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    for service in config[CONF_SERVICES]:
        txt = [
            cg.StructInitializer(
                MDNSTXTRecord,
                ("key", txt_key),
                ("value", await cg.templatable(txt_value, [], cg.std_string)),
            )
            for txt_key, txt_value in service[CONF_TXT].items()
        ]
        exp = mdns_service(
            service[CONF_SERVICE],
            service[CONF_PROTOCOL],
            await cg.templatable(service[CONF_PORT], [], cg.uint16),
            txt,
        )

        cg.add(var.add_extra_service(exp))


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "mdns_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "mdns_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
        "mdns_host.cpp": {PlatformFramework.HOST_NATIVE},
        "mdns_rp2040.cpp": {PlatformFramework.RP2040_ARDUINO},
        "mdns_libretiny.cpp": {
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
    }
)
