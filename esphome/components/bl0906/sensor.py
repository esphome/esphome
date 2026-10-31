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
    ICON_THERMOMETER,
)
from esphome.types import ConfigType

# Import ICONS not included in esphome's const.py, from the local components const.py
from .const import ICON_FREQUENCY

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["bl0906"]
CONF_TOTAL_ENERGY = "total_energy"

bl0906_ns = cg.esphome_ns.namespace("bl0906")
BL0906 = bl0906_ns.class_("BL0906", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BL0906),
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                icon=ICON_FREQUENCY, accuracy_decimals=0, kind=sensor.FREQUENCY
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                icon=ICON_THERMOMETER, accuracy_decimals=0, kind=sensor.TEMPERATURE
            ),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                accuracy_decimals=1, kind=sensor.VOLTAGE
            ),
            cv.Optional(CONF_TOTAL_POWER): sensor.sensor_schema(
                accuracy_decimals=3, kind=sensor.POWER
            ),
            cv.Optional(CONF_TOTAL_ENERGY): sensor.sensor_schema(
                accuracy_decimals=3, kind=sensor.ENERGY_KWH
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
                                accuracy_decimals=3, kind=sensor.CURRENT
                            ),
                            key=CONF_NAME,
                        ),
                        cv.Optional(CONF_POWER): cv.maybe_simple_value(
                            sensor.sensor_schema(
                                accuracy_decimals=0, kind=sensor.POWER
                            ),
                            key=CONF_NAME,
                        ),
                        cv.Optional(CONF_ENERGY): cv.maybe_simple_value(
                            sensor.sensor_schema(
                                accuracy_decimals=3, kind=sensor.ENERGY_KWH
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


automation.register_apply_action(
    "bl0906.reset_energy",
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(BL0906),
        }
    ),
    automation.ApplyCall("reset_energy()"),
)


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_FREQUENCY, var.set_frequency_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)

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

    await sensors(CONF_TOTAL_POWER, var.set_total_power_sensor)
    await sensors(CONF_TOTAL_ENERGY, var.set_total_energy_sensor)
