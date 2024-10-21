import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_IP_ADDRESS, ENTITY_CATEGORY_DIAGNOSTIC

CONF_ROLE = "role"
CONF_RLOC16 = "rloc16"
CONF_EUI64 = "eui64"
CONF_EXTADDR = "extaddr"

# TODO: Move these to const.py
CONF_NETWORK_NAME = "network_name"
CONF_NETWORK_KEY = "network_key"
CONF_PSKC = "pskc"
CONF_PANID = "panid"
CONF_EXTPANID = "extpanid"


DEPENDENCIES = ["openthread"]

openthread_info_ns = cg.esphome_ns.namespace("openthread_info")
IPAddressOpenThreadInfo = openthread_info_ns.class_(
    "IPAddressOpenThreadInfo", text_sensor.TextSensor, cg.PollingComponent
)
RoleOpenThreadInfo = openthread_info_ns.class_(
    "RoleOpenThreadInfo", text_sensor.TextSensor, cg.PollingComponent
)
Rloc16OpenThreadInfo = openthread_info_ns.class_(
    "Rloc16OpenThreadInfo", text_sensor.TextSensor, cg.PollingComponent
)
ExtAddrOpenThreadInfo = openthread_info_ns.class_(
    "ExtAddrOpenThreadInfo", text_sensor.TextSensor, cg.PollingComponent
)
Eui64OpenThreadInfo = openthread_info_ns.class_(
    "Eui64OpenThreadInfo", text_sensor.TextSensor, cg.Component
)
ChannelOpenThreadInfo = openthread_info_ns.class_(
    "ChannelOpenThreadInfo", text_sensor.TextSensor, cg.PollingComponent
)
NetworkNameOpenThreadInfo = openthread_info_ns.class_(
    "NetworkNameOpenThreadInfo", text_sensor.TextSensor, cg.PollingComponent
)
NetworkKeyOpenThreadInfo = openthread_info_ns.class_(
    "NetworkKeyOpenThreadInfo", text_sensor.TextSensor, cg.PollingComponent
)
PanIdOpenThreadInfo = openthread_info_ns.class_(
    "PanIdOpenThreadInfo", text_sensor.TextSensor, cg.PollingComponent
)
ExtPanIdOpenThreadInfo = openthread_info_ns.class_(
    "ExtPanIdOpenThreadInfo", text_sensor.TextSensor, cg.PollingComponent
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_IP_ADDRESS): text_sensor.text_sensor_schema(
            IPAddressOpenThreadInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_ROLE): text_sensor.text_sensor_schema(
            RoleOpenThreadInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s")),
        cv.Optional(CONF_RLOC16): text_sensor.text_sensor_schema(
            Rloc16OpenThreadInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s")),
        cv.Optional(CONF_EXTADDR): text_sensor.text_sensor_schema(
            ExtAddrOpenThreadInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s")),
        cv.Optional(CONF_EUI64): text_sensor.text_sensor_schema(
            Eui64OpenThreadInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1h")),
        cv.Optional(CONF_CHANNEL): text_sensor.text_sensor_schema(
            ChannelOpenThreadInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s")),
        cv.Optional(CONF_NETWORK_NAME): text_sensor.text_sensor_schema(
            NetworkNameOpenThreadInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s")),
        cv.Optional(CONF_NETWORK_KEY): text_sensor.text_sensor_schema(
            NetworkKeyOpenThreadInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s")),
        cv.Optional(CONF_PANID): text_sensor.text_sensor_schema(
            PanIdOpenThreadInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s")),
        cv.Optional(CONF_EXTPANID): text_sensor.text_sensor_schema(
            ExtPanIdOpenThreadInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s")),
    }
)


async def setup_conf(config, key):
    if key in config:
        conf = config[key]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)


async def to_code(config):
    await setup_conf(config, CONF_IP_ADDRESS)
    await setup_conf(config, CONF_ROLE)
    await setup_conf(config, CONF_RLOC16)
    await setup_conf(config, CONF_EXTADDR)
    await setup_conf(config, CONF_EUI64)
    await setup_conf(config, CONF_CHANNEL)
    await setup_conf(config, CONF_NETWORK_NAME)
    await setup_conf(config, CONF_NETWORK_KEY)
    await setup_conf(config, CONF_PANID)
    await setup_conf(config, CONF_EXTPANID)
