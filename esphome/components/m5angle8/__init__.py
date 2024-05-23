import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor, binary_sensor, light
from esphome.automation import maybe_simple_id


AUTO_LOAD = ["sensor", "light"]
DEPENDENCIES = ["i2c"]
CODEOWNERS = [
    "@rnauber",
]
MULTI_CONF = True

NUMBER_KNOBS = 8
NUMBER_LEDS = 9

from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    ICON_ROTATE_RIGHT,
    CONF_CHANNEL,
    CONF_OUTPUT_ID,
)


CONF_KNOB_POSITION_PREFIX = "knob_position_"
CONF_INPUT_SWITCH = "input_switch"
CONF_LIGHTS = "lights"

m5angle8_ns = cg.esphome_ns.namespace("m5angle8")
M5Angle8Component = m5angle8_ns.class_(
    "M5Angle8Component",
    i2c.I2CDevice,
    cg.Component,
)

M5Angle8LightsComponent = m5angle8_ns.class_(
    "M5Angle8LightOutput",
    light.AddressableLight,
)

M5Angle8SensorKnob = m5angle8_ns.class_(
    "M5Angle8SensorKnob",
    sensor.Sensor,
    cg.PollingComponent,
)

M5Angle8SensorSwitch = m5angle8_ns.class_(
    "M5Angle8SensorSwitch",
    binary_sensor.BinarySensor,
    cg.PollingComponent,
)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5Angle8Component),
            cv.Optional(CONF_LIGHTS): light.ADDRESSABLE_LIGHT_SCHEMA.extend(
                {
                    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(
                        M5Angle8LightsComponent
                    ),
                }
            ),
        }
    )
    .extend(i2c.i2c_device_schema(0x43))
    .extend(
        {
            cv.Optional(CONF_KNOB_POSITION_PREFIX + str(i + 1)): sensor.sensor_schema(
                M5Angle8SensorKnob,
                accuracy_decimals=2,
                icon=ICON_ROTATE_RIGHT,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            .extend({cv.GenerateID(): cv.declare_id(M5Angle8SensorKnob)})
            .extend(cv.polling_component_schema("10s"))
            for i in range(NUMBER_KNOBS)
        }
    )
    .extend(
        {
            cv.Optional(CONF_INPUT_SWITCH): binary_sensor.binary_sensor_schema(
                M5Angle8SensorSwitch,
            )
            .extend({cv.GenerateID(): cv.declare_id(M5Angle8SensorSwitch)})
            .extend(cv.polling_component_schema("10s"))
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if lights_config := config.get(CONF_LIGHTS):
        lights = cg.new_Pvariable(lights_config[CONF_OUTPUT_ID])
        await light.register_light(lights, lights_config)
        await cg.register_component(lights, lights_config)
        cg.add(lights.set_parent(var))

    for i in range(NUMBER_KNOBS):
        conf_key = CONF_KNOB_POSITION_PREFIX + str(i + 1)
        if knob_config := config.get(conf_key):
            sens = await sensor.new_sensor(knob_config)
            cg.add(sens.set_parent(var, i))
            await cg.register_component(sens, knob_config)

    if sw_config := config.get(CONF_INPUT_SWITCH):
        sens = await binary_sensor.new_binary_sensor(sw_config)
        cg.add(sens.set_parent(var))
        await cg.register_component(sens, sw_config)
