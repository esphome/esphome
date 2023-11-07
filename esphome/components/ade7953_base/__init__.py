import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome import pins
from esphome.const import (
    CONF_IRQ_PIN,
    CONF_VOLTAGE,
    CONF_FREQUENCY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_FREQUENCY,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
    UNIT_HERTZ,
    UNIT_AMPERE,
    UNIT_VOLT_AMPS,
    UNIT_WATT,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_PERCENT,
)

CONF_CURRENT_A = "current_a"
CONF_CURRENT_B = "current_b"
CONF_ACTIVE_POWER_A = "active_power_a"
CONF_ACTIVE_POWER_B = "active_power_b"
CONF_APPARENT_POWER_A = "apparent_power_a"
CONF_APPARENT_POWER_B = "apparent_power_b"
CONF_REACTIVE_POWER_A = "reactive_power_a"
CONF_REACTIVE_POWER_B = "reactive_power_b"
CONF_POWER_FACTOR_A = "power_factor_a"
CONF_POWER_FACTOR_B = "power_factor_b"
CONF_VOLTAGE_PGA_GAIN = "voltage_pga_gain"
CONF_CURRENT_PGA_GAIN_A = "current_pga_gain_a"
CONF_CURRENT_PGA_GAIN_B = "current_pga_gain_b"
CONF_VOLTAGE_GAIN = "voltage_gain"
CONF_CURRENT_GAIN_A = "current_gain_a"
CONF_CURRENT_GAIN_B = "current_gain_b"
CONF_ACTIVE_POWER_GAIN_A = "active_power_gain_a"
CONF_ACTIVE_POWER_GAIN_B = "active_power_gain_b"
PGA_GAINS = {
    "1x": 0b000,
    "2x": 0b001,
    "4x": 0b010,
    "8x": 0b011,
    "16x": 0b100,
    "22x": 0b101,
}

ade7953_base_ns = cg.esphome_ns.namespace("ade7953_base")
ADE7953 = ade7953_base_ns.class_("ADE7953", cg.PollingComponent)

ADE7953_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_IRQ_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CURRENT_A): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CURRENT_B): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ACTIVE_POWER_A): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ACTIVE_POWER_B): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_APPARENT_POWER_A): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT_AMPS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_APPARENT_POWER_B): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT_AMPS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_REACTIVE_POWER_A): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_REACTIVE_POWER_B): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER_FACTOR_A): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_POWER_FACTOR,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER_FACTOR_B): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_POWER_FACTOR,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            CONF_VOLTAGE_PGA_GAIN,
            default="1x",
        ): cv.one_of(*PGA_GAINS, lower=True),
        cv.Optional(
            CONF_CURRENT_PGA_GAIN_A,
            default="1x",
        ): cv.one_of(*PGA_GAINS, lower=True),
        cv.Optional(
            CONF_CURRENT_PGA_GAIN_B,
            default="1x",
        ): cv.one_of(*PGA_GAINS, lower=True),
        cv.Optional(CONF_VOLTAGE_GAIN, default=0x400000): cv.hex_int_range(
            min=0x100000, max=0x800000
        ),
        cv.Optional(CONF_CURRENT_GAIN_A, default=0x400000): cv.hex_int_range(
            min=0x100000, max=0x800000
        ),
        cv.Optional(CONF_CURRENT_GAIN_B, default=0x400000): cv.hex_int_range(
            min=0x100000, max=0x800000
        ),
        cv.Optional(CONF_ACTIVE_POWER_GAIN_A, default=0x400000): cv.hex_int_range(
            min=0x100000, max=0x800000
        ),
        cv.Optional(CONF_ACTIVE_POWER_GAIN_B, default=0x400000): cv.hex_int_range(
            min=0x100000, max=0x800000
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def register_ade7953(var, config):
    await cg.register_component(var, config)

    if irq_pin_config := config.get(CONF_IRQ_PIN):
        irq_pin = await cg.gpio_pin_expression(irq_pin_config)
        cg.add(var.set_irq_pin(irq_pin))

    cg.add(var.set_pga_v(PGA_GAINS[config.get(CONF_VOLTAGE_PGA_GAIN)]))
    cg.add(var.set_pga_ia(PGA_GAINS[config.get(CONF_CURRENT_PGA_GAIN_A)]))
    cg.add(var.set_pga_ib(PGA_GAINS[config.get(CONF_CURRENT_PGA_GAIN_B)]))
    cg.add(var.set_vgain(config.get(CONF_VOLTAGE_GAIN)))
    cg.add(var.set_aigain(config.get(CONF_CURRENT_GAIN_A)))
    cg.add(var.set_bigain(config.get(CONF_CURRENT_GAIN_B)))
    cg.add(var.set_awgain(config.get(CONF_ACTIVE_POWER_GAIN_A)))
    cg.add(var.set_bwgain(config.get(CONF_ACTIVE_POWER_GAIN_B)))

    for key in [
        CONF_VOLTAGE,
        CONF_FREQUENCY,
        CONF_CURRENT_A,
        CONF_CURRENT_B,
        CONF_POWER_FACTOR_A,
        CONF_POWER_FACTOR_B,
        CONF_APPARENT_POWER_A,
        CONF_APPARENT_POWER_B,
        CONF_ACTIVE_POWER_A,
        CONF_ACTIVE_POWER_B,
        CONF_REACTIVE_POWER_A,
        CONF_REACTIVE_POWER_B,
    ]:
        if key not in config:
            continue
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(var, f"set_{key}_sensor")(sens))
