from esphome import pins
import esphome.codegen as cg
from esphome.components import binary_sensor, sensor, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_FILTER,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_PIN,
    CONF_SENSOR_ID,
    CONF_TEMPERATURE,
    CONF_WIND_DIRECTION_DEGREES,
    CONF_WIND_SPEED,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_WIND_DIRECTION,
    DEVICE_CLASS_WIND_SPEED,
    ICON_COUNTER,
    ICON_GRAIN,
    ICON_SIGN_DIRECTION,
    ICON_WEATHER_WINDY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_DEGREES,
    UNIT_LUX,
    UNIT_METER_PER_SECOND,
    UNIT_MILLIMETER,
    UNIT_PERCENT,
)
from esphome.core import TimePeriod

AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
CODEOWNERS = ["@Bofr5555"]

CONF_BIT_TIME = "bit_time"
CONF_LOW_BATTERY = "low_battery"
CONF_MAX_GAP = "max_gap"
CONF_RAIN = "rain"
CONF_TX_COUNTER = "tx_counter"
CONF_UV_INDEX = "uv_index"
CONF_WIND_GUST = "wind_gust"

vevor_dtx_ns = cg.esphome_ns.namespace("vevor_dtx")
VevorDtxComponent = vevor_dtx_ns.class_("VevorDtxComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VevorDtxComponent),
        cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema),
        cv.Optional(CONF_BIT_TIME, default="88us"): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(
                min=TimePeriod(microseconds=50),
                max=TimePeriod(microseconds=150),
            ),
        ),
        cv.Optional(CONF_MAX_GAP, default="2000us"): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(
                min=TimePeriod(microseconds=500),
                max=TimePeriod(microseconds=20000),
            ),
        ),
        cv.Optional(CONF_FILTER, default="20us"): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(
                min=TimePeriod(microseconds=0),
                max=TimePeriod(microseconds=500),
            ),
        ),
        cv.Optional(CONF_BUFFER_SIZE, default="512b"): cv.All(
            cv.validate_bytes,
            cv.Range(min=128, max=2048),
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_WIND_SPEED): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER_PER_SECOND,
            icon=ICON_WEATHER_WINDY,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_WIND_SPEED,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_WIND_GUST): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER_PER_SECOND,
            icon=ICON_WEATHER_WINDY,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_WIND_SPEED,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_WIND_DIRECTION_DEGREES): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            icon=ICON_SIGN_DIRECTION,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_WIND_DIRECTION,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RAIN): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIMETER,
            icon=ICON_GRAIN,
            accuracy_decimals=1,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_UV_INDEX): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_LUX,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SENSOR_ID): text_sensor.text_sensor_schema(
            icon="mdi:identifier",
        ),
        cv.Optional(CONF_TX_COUNTER): sensor.sensor_schema(
            accuracy_decimals=0,
            icon=ICON_COUNTER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LOW_BATTERY): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_BATTERY
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_bit_time_us(config[CONF_BIT_TIME]))
    cg.add(var.set_max_gap_us(config[CONF_MAX_GAP]))
    cg.add(var.set_filter_us(config[CONF_FILTER]))
    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))

    for key, setter in (
        (CONF_TEMPERATURE, "set_temperature_sensor"),
        (CONF_HUMIDITY, "set_humidity_sensor"),
        (CONF_WIND_SPEED, "set_wind_speed_sensor"),
        (CONF_WIND_GUST, "set_wind_gust_sensor"),
        (CONF_WIND_DIRECTION_DEGREES, "set_wind_direction_sensor"),
        (CONF_RAIN, "set_rain_sensor"),
        (CONF_UV_INDEX, "set_uv_index_sensor"),
        (CONF_ILLUMINANCE, "set_illuminance_sensor"),
        (CONF_TX_COUNTER, "set_tx_counter_sensor"),
    ):
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(var, setter)(sens))

    if (conf := config.get(CONF_SENSOR_ID)) is not None:
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(var.set_sensor_id_text_sensor(sens))

    if (conf := config.get(CONF_LOW_BATTERY)) is not None:
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_low_battery_binary_sensor(sens))
