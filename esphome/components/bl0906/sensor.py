from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_CURRENT,
    CONF_ENERGY,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_NAME,
    CONF_POWER,
    CONF_TEMPERATURE,
    CONF_TOTAL_POWER,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    ICON_POWER,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

# Import ICONS not included in esphome's const.py, from the local components const.py
from .const import ICON_ENERGY, ICON_FREQUENCY, ICON_VOLTAGE

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["bl0906"]
CONF_TOTAL_ENERGY = "total_energy"

bl0906_ns = cg.esphome_ns.namespace("bl0906")
BL0906 = bl0906_ns.class_("BL0906", cg.PollingComponent, uart.UARTDevice)
ResetEnergyAction = bl0906_ns.class_("ResetEnergyAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BL0906),
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                icon=ICON_FREQUENCY,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_FREQUENCY,
                unit_of_measurement=UNIT_HERTZ,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                unit_of_measurement=UNIT_CELSIUS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                icon=ICON_VOLTAGE,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                unit_of_measurement=UNIT_VOLT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TOTAL_POWER): sensor.sensor_schema(
                icon=ICON_POWER,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_POWER,
                unit_of_measurement=UNIT_WATT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TOTAL_ENERGY): sensor.sensor_schema(
                icon=ICON_ENERGY,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                unit_of_measurement=UNIT_KILOWATT_HOURS,
            ),
        }
    )
    .extend(
        cv.Schema(
            {
                cv.Optional(f"{CONF_CHANNEL}_{i + 1}"): cv.Schema(
                    {
                        cv.Optional(CONF_CURRENT): cv.maybe_simple_value(
                            sensor.sensor_schema(
                                icon=ICON_CURRENT_AC,
                                accuracy_decimals=3,
                                device_class=DEVICE_CLASS_CURRENT,
                                unit_of_measurement=UNIT_AMPERE,
                                state_class=STATE_CLASS_MEASUREMENT,
                            ),
                            key=CONF_NAME,
                        ),
                        cv.Optional(CONF_POWER): cv.maybe_simple_value(
                            sensor.sensor_schema(
                                icon=ICON_POWER,
                                accuracy_decimals=0,
                                device_class=DEVICE_CLASS_POWER,
                                unit_of_measurement=UNIT_WATT,
                                state_class=STATE_CLASS_MEASUREMENT,
                            ),
                            key=CONF_NAME,
                        ),
                        cv.Optional(CONF_ENERGY): cv.maybe_simple_value(
                            sensor.sensor_schema(
                                icon=ICON_ENERGY,
                                accuracy_decimals=3,
                                device_class=DEVICE_CLASS_ENERGY,
                                unit_of_measurement=UNIT_KILOWATT_HOURS,
                                state_class=STATE_CLASS_TOTAL_INCREASING,
                            ),
                            key=CONF_NAME,
                        ),
                    }
                )
                for i in range(6)
            }
        )
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "bl0906", baud_rate=19200, require_tx=True, require_rx=True
)


@automation.register_action(
    "bl0906.reset_energy",
    ResetEnergyAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(BL0906),
        }
    ),
)
async def reset_energy_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if frequency_config := config.get(CONF_FREQUENCY):
        sens = await sensor.new_sensor(frequency_config)
        cg.add(var.set_frequency_sensor(sens))
    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))
    if voltage_config := config.get(CONF_VOLTAGE):
        sens = await sensor.new_sensor(voltage_config)
        cg.add(var.set_voltage_sensor(sens))

    for i in range(6):
        if channel_config := config.get(f"{CONF_CHANNEL}_{i + 1}"):
            if current_config := channel_config.get(CONF_CURRENT):
                sens = await sensor.new_sensor(current_config)
                cg.add(getattr(var, f"set_current_{i + 1}_sensor")(sens))
            if power_config := channel_config.get(CONF_POWER):
                sens = await sensor.new_sensor(power_config)
                cg.add(getattr(var, f"set_power_{i + 1}_sensor")(sens))
            if energy_config := channel_config.get(CONF_ENERGY):
                sens = await sensor.new_sensor(energy_config)
                cg.add(getattr(var, f"set_energy_{i + 1}_sensor")(sens))

    if total_power_config := config.get(CONF_TOTAL_POWER):
        sens = await sensor.new_sensor(total_power_config)
        cg.add(var.set_total_power_sensor(sens))

    if total_energy_config := config.get(CONF_TOTAL_ENERGY):
        sens = await sensor.new_sensor(total_energy_config)
        cg.add(var.set_total_energy_sensor(sens))
