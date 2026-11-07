import esphome.codegen as cg
from esphome.components import sensor, wifi
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

DEPENDENCIES = ["wifi"]

wifi_info_ns = cg.esphome_ns.namespace("wifi_info")
ChannelWiFiInfo = wifi_info_ns.class_("ChannelWiFiInfo", sensor.Sensor, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CHANNEL): sensor.sensor_schema(
            ChannelWiFiInfo,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    if conf := config.get(CONF_CHANNEL):
        wifi.request_wifi_connect_state_listener()
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)
