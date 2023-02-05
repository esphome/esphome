import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, fluval_ble_led
from .. import fluval_ble_led_ns

FluvalBleModeSensor = fluval_ble_led_ns.class_(
    "FluvalBleModeSensor", text_sensor.TextSensor, cg.Component, fluval_ble_led.FluvalBleLed
)

CONF_MAPPING_MANUAL="mapping_manual"
CONF_MAPPING_AUTO="mapping_auto"
CONF_MAPPING_PRO="mapping_pro"

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema()
    .extend(
        {
            cv.GenerateID(): cv.declare_id(FluvalBleModeSensor),
            cv.Optional(CONF_MAPPING_MANUAL, "manual"): str,
            cv.Optional(CONF_MAPPING_AUTO, "auto"): str,
            cv.Optional(CONF_MAPPING_PRO, "pro"): str

        }
    )
    .extend(fluval_ble_led.FLUVAL_CLIENT_SCHEMA)
)

async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_manual_mapping(config[CONF_MAPPING_MANUAL]))
    cg.add(var.set_auto_mapping(config[CONF_MAPPING_AUTO]))
    cg.add(var.set_pro_mapping(config[CONF_MAPPING_PRO]))
    await fluval_ble_led.register_fluval_led_client(var, config)

