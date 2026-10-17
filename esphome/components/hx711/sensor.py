from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_CLK_PIN, CONF_GAIN, ICON_SCALE, STATE_CLASS_MEASUREMENT
from esphome.types import ConfigType

CODEOWNERS = ["@DjordjeMandic"]

hx711_ns = cg.esphome_ns.namespace("hx711")
HX711Sensor = hx711_ns.class_("HX711Sensor", sensor.Sensor, cg.PollingComponent)

CONF_DOUT_PIN = "dout_pin"
CONF_SETTLING_TIME = "settling_time"
CONF_POWER_DOWN_AFTER_READING = "power_down_after_reading"
CONF_CHANNEL_B = "channel_b"
CONF_MEASUREMENT_READY_TIMEOUT = "measurement_ready_timeout"

HX711Gain = hx711_ns.enum("HX711Gain")
GAINS = {
    128: HX711Gain.HX711_GAIN_128,
    32: HX711Gain.HX711_GAIN_32,
    64: HX711Gain.HX711_GAIN_64,
}

_milliseconds_uint16 = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(max=cv.TimePeriod(milliseconds=65535)),
)

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
            ): _milliseconds_uint16,
            cv.Optional(CONF_SETTLING_TIME, default="400ms"): _milliseconds_uint16,
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


async def to_code(config: ConfigType) -> None:
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
    sensors = sensor.sub_sensors(config)
    if await sensors(CONF_CHANNEL_B, var.set_channel_b_sensor):
        cg.add_define("USE_HX711_CHANNEL_B_SENSOR")


HX711_ACTION_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(HX711Sensor)})

automation.register_apply_action(
    "hx711.power_up", HX711_ACTION_SCHEMA, automation.ApplyCall("power_up()")
)
automation.register_apply_action(
    "hx711.power_down", HX711_ACTION_SCHEMA, automation.ApplyCall("power_down()")
)
automation.register_apply_action(
    "hx711.set_gain",
    HX711_ACTION_SCHEMA.extend(
        {cv.Required(CONF_GAIN): cv.templatable(cv.enum(GAINS, int=True))}
    ),
    automation.ApplyField(CONF_GAIN, "set_gain", HX711Gain),
)
