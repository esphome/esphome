import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_IP_ADDRESS,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

DEPENDENCIES = ["ethernet"]

ethernet_info_ns = cg.esphome_ns.namespace("ethernet_info")

IPAddressEsthernetInfo = ethernet_info_ns.class_(
    "IPAddressEthernetInfo", text_sensor.TextSensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_IP_ADDRESS): text_sensor.text_sensor_schema(
            IPAddressEsthernetInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s"))
    }
)


async def setup_conf(config, key):
    if key in config:
        conf = config[key]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)


async def to_code(config):
    await setup_conf(config, CONF_IP_ADDRESS)
