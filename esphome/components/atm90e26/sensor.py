import esphome.codegen as cg
from esphome.components import sensor, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_FORWARD_ACTIVE_ENERGY,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_LINE_FREQUENCY,
    CONF_POWER,
    CONF_POWER_FACTOR,
    CONF_REACTIVE_POWER,
    CONF_REVERSE_ACTIVE_ENERGY,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    ICON_LIGHTBULB,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_HERTZ,
    UNIT_VOLT,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

CONF_METER_CONSTANT = "meter_constant"
CONF_PL_CONST = "pl_const"
CONF_GAIN_PGA = "gain_pga"
CONF_GAIN_METERING = "gain_metering"
CONF_GAIN_VOLTAGE = "gain_voltage"
CONF_GAIN_CT = "gain_ct"
LINE_FREQS = {
    "50HZ": 50,
    "60HZ": 60,
}
PGA_GAINS = {
    "1X": 0x4,
    "4X": 0x0,
    "8X": 0x1,
    "16X": 0x2,
    "24X": 0x3,
}

atm90e26_ns = cg.esphome_ns.namespace("atm90e26")
ATM90E26Component = atm90e26_ns.class_(
    "ATM90E26Component", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ATM90E26Component),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_REACTIVE_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
                icon=ICON_LIGHTBULB,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER_FACTOR): sensor.sensor_schema(
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER_FACTOR,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FORWARD_ACTIVE_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT_HOURS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_REVERSE_ACTIVE_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT_HOURS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                icon=ICON_CURRENT_AC,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Required(CONF_LINE_FREQUENCY): cv.enum(LINE_FREQS, upper=True),
            cv.Required(CONF_METER_CONSTANT): cv.positive_float,
            cv.Optional(CONF_PL_CONST, default=1429876): cv.uint32_t,
            cv.Optional(CONF_GAIN_METERING, default=7481): cv.uint16_t,
            cv.Optional(CONF_GAIN_VOLTAGE, default=26400): cv.int_range(
                min=0, max=32767
            ),
            cv.Optional(CONF_GAIN_CT, default=31251): cv.uint16_t,
            cv.Optional(CONF_GAIN_PGA, default="1X"): cv.enum(PGA_GAINS, upper=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema())
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if voltage_config := config.get(CONF_VOLTAGE):
        sens = await sensor.new_sensor(voltage_config)
        cg.add(var.set_voltage_sensor(sens))
    if current_config := config.get(CONF_CURRENT):
        sens = await sensor.new_sensor(current_config)
        cg.add(var.set_current_sensor(sens))
    if power_config := config.get(CONF_POWER):
        sens = await sensor.new_sensor(power_config)
        cg.add(var.set_power_sensor(sens))
    if reactive_power_config := config.get(CONF_REACTIVE_POWER):
        sens = await sensor.new_sensor(reactive_power_config)
        cg.add(var.set_reactive_power_sensor(sens))
    if power_factor_config := config.get(CONF_POWER_FACTOR):
        sens = await sensor.new_sensor(power_factor_config)
        cg.add(var.set_power_factor_sensor(sens))
    if forward_active_energy_config := config.get(CONF_FORWARD_ACTIVE_ENERGY):
        sens = await sensor.new_sensor(forward_active_energy_config)
        cg.add(var.set_forward_active_energy_sensor(sens))
    if reverse_active_energy_config := config.get(CONF_REVERSE_ACTIVE_ENERGY):
        sens = await sensor.new_sensor(reverse_active_energy_config)
        cg.add(var.set_reverse_active_energy_sensor(sens))
    if frequency_config := config.get(CONF_FREQUENCY):
        sens = await sensor.new_sensor(frequency_config)
        cg.add(var.set_freq_sensor(sens))
    cg.add(var.set_line_freq(config[CONF_LINE_FREQUENCY]))
    cg.add(var.set_meter_constant(config[CONF_METER_CONSTANT]))
    cg.add(var.set_pl_const(config[CONF_PL_CONST]))
    cg.add(var.set_gain_metering(config[CONF_GAIN_METERING]))
    cg.add(var.set_gain_voltage(config[CONF_GAIN_VOLTAGE]))
    cg.add(var.set_gain_ct(config[CONF_GAIN_CT]))
    cg.add(var.set_gain_pga(config[CONF_GAIN_PGA]))
