import esphome.codegen as cg
from esphome.components import sensor, wifi
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

DEPENDENCIES = ["wifi"]
wifi_signal_ns = cg.esphome_ns.namespace("wifi_signal")
WiFiSignalSensor = wifi_signal_ns.class_(
    "WiFiSignalSensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = sensor.sensor_schema(
    WiFiSignalSensor,
    kind=sensor.SIGNAL_STRENGTH,
    accuracy_decimals=0,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(cv.polling_component_schema("60s"))


async def to_code(config: ConfigType) -> None:
    wifi.request_wifi_connect_state_listener()
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
