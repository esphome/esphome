import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, fluval_ble_led
from esphome.const import DEVICE_CLASS_EMPTY, STATE_CLASS_NONE, ICON_LIGHTBULB
from .. import fluval_ble_led_ns

CONF_CHANNEL = "channel"
CONF_ZERO_IF_OFF = "zero_if_off"

FluvalBleChannelSensor = fluval_ble_led_ns.class_(
    "FluvalBleChannelSensor", sensor.Sensor, cg.Component, fluval_ble_led.FluvalBleLed
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(FluvalBleChannelSensor, accuracy_decimals=0, device_class=DEVICE_CLASS_EMPTY, state_class=STATE_CLASS_NONE, icon=ICON_LIGHTBULB)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(FluvalBleChannelSensor),
            cv.Required(CONF_CHANNEL): int,
            cv.Optional(CONF_ZERO_IF_OFF, False): bool
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(fluval_ble_led.FLUVAL_CLIENT_SCHEMA)
)

async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_zero_if_off(config[CONF_ZERO_IF_OFF]))
    await fluval_ble_led.register_fluval_led_client(var, config)
