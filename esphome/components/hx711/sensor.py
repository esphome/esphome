from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLK_PIN,
    CONF_GAIN,
    CONF_ID,
    ICON_SCALE,
    STATE_CLASS_MEASUREMENT,
)

CODEOWNERS = ["@DjordjeMandic"]

hx711_ns = cg.esphome_ns.namespace("hx711")
HX711Sensor = hx711_ns.class_("HX711Sensor", sensor.Sensor, cg.PollingComponent)

CONF_DOUT_PIN = "dout_pin"
CONF_SETTLING_TIME = "settling_time"
CONF_POWER_DOWN_AFTER_READING = "power_down_after_reading"
CONF_CHANNEL_B = "channel_b"
CONF_MEASUREMENT_READY_TIMEOUT = "measurement_ready_timeout"

HX711Gain = hx711_ns.enum("HX711Gain", is_class=True)
GAINS = {
    128: HX711Gain.HX711_GAIN_128,
    32: HX711Gain.HX711_GAIN_32,
    64: HX711Gain.HX711_GAIN_64,
}

PowerUpAction = hx711_ns.class_("PowerUpAction", automation.Action)
PowerDownAction = hx711_ns.class_("PowerDownAction", automation.Action)
SetGainAction = hx711_ns.class_("SetGainAction", automation.Action)

HX711_ACTION_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(HX711Sensor),
    }
)

HX711_SET_GAIN_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GAIN): cv.templatable(cv.enum(GAINS, int=True)),
    }
).extend(HX711_ACTION_BASE_SCHEMA)


positive_time_period_milliseconds_uint16_validator = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(max=cv.TimePeriod(milliseconds=65535)),
)

# CONF_CLK_PIN needs to be readable/input to check for power down state
CONFIG_SCHEMA = (
    sensor.sensor_schema(
        HX711Sensor,
        icon=ICON_SCALE,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_DOUT_PIN): pins.gpio_input_pin_schema,
            cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_GAIN, default=128): cv.enum(GAINS, int=True),
            cv.Optional(
                CONF_MEASUREMENT_READY_TIMEOUT, default="2000ms"
            ): positive_time_period_milliseconds_uint16_validator,
            cv.Optional(
                CONF_SETTLING_TIME, default="400ms"
            ): positive_time_period_milliseconds_uint16_validator,
            cv.Optional(CONF_POWER_DOWN_AFTER_READING, default=False): cv.boolean,
            cv.Optional(CONF_CHANNEL_B): sensor.sensor_schema(
                icon=ICON_SCALE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    dout_pin = await cg.gpio_pin_expression(config[CONF_DOUT_PIN])
    cg.add(var.set_dout_pin(dout_pin))
    sck_pin = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_sck_pin(sck_pin))
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_measurement_ready_timeout(config[CONF_MEASUREMENT_READY_TIMEOUT]))
    cg.add(var.set_settling_time(config[CONF_SETTLING_TIME]))
    cg.add(var.set_power_down_after_reading(config[CONF_POWER_DOWN_AFTER_READING]))
    if channel_b_config := config.get(CONF_CHANNEL_B):
        cg.add_define("USE_HX711_CHANNEL_B_SENSOR")
        sens = await sensor.new_sensor(channel_b_config)
        cg.add(var.set_channel_b_sensor(sens))


async def hx711_action_to_code(config, action_id, template_arg):
    hx711_ = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_parent(hx711_))
    return var


@automation.register_action("hx711.power_up", PowerUpAction, HX711_ACTION_BASE_SCHEMA)
async def hx711_power_up_to_code(config, action_id, template_arg, args):
    return await hx711_action_to_code(config, action_id, template_arg)


@automation.register_action(
    "hx711.power_down", PowerDownAction, HX711_ACTION_BASE_SCHEMA
)
async def hx711_power_down_to_code(config, action_id, template_arg, args):
    return await hx711_action_to_code(config, action_id, template_arg)


@automation.register_action(
    "hx711.set_gain", SetGainAction, HX711_SET_GAIN_ACTION_SCHEMA
)
async def hx711_set_gain_to_code(config, action_id, template_arg, args):
    var = await hx711_action_to_code(config, action_id, template_arg)
    gain_ = await cg.templatable(config[CONF_GAIN], args, HX711Gain)
    cg.add(var.set_gain(gain_))
    return var
