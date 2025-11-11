"""Support for Sonoff SPM (Smart Power Manager)."""

import esphome.codegen as cg
from esphome.components import sensor, switch, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_ENERGY,
    CONF_ID,
    CONF_POWER,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

CODEOWNERS = ["@fhedberg"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["switch", "sensor"]
MULTI_CONF = False

CONF_SONOFF_SPM_ID = "sonoff_spm_id"
CONF_MODULE_COUNT = "module_count"
CONF_AUTO_CREATE_SWITCHES = "auto_create_switches"
CONF_AUTO_CREATE_SENSORS = "auto_create_sensors"
CONF_NAME_PREFIX = "name_prefix"
CONF_SENSOR_TYPES = "sensor_types"

sonoff_spm_ns = cg.esphome_ns.namespace("sonoff_spm")
SonoffSPM = sonoff_spm_ns.class_("SonoffSPM", cg.Component, uart.UARTDevice)
SonoffSPMSwitch = sonoff_spm_ns.class_("SonoffSPMSwitch", switch.Switch, cg.Component)
SonoffSPMSensor = sonoff_spm_ns.class_("SonoffSPMSensor", cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SonoffSPM),
            cv.Optional(CONF_MODULE_COUNT, default=32): cv.int_range(min=1, max=32),
            cv.Optional(CONF_AUTO_CREATE_SWITCHES, default=False): cv.boolean,
            cv.Optional(CONF_AUTO_CREATE_SENSORS, default=False): cv.boolean,
            cv.Optional(CONF_NAME_PREFIX, default="SPM Relay"): cv.string,
            cv.Optional(
                CONF_SENSOR_TYPES,
                default=[CONF_VOLTAGE, CONF_CURRENT, CONF_POWER, CONF_ENERGY],
            ): cv.ensure_list(
                cv.one_of(
                    CONF_VOLTAGE, CONF_CURRENT, CONF_POWER, CONF_ENERGY, lower=True
                )
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    """Generate code for Sonoff SPM component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_module_count(config[CONF_MODULE_COUNT]))

    # Auto-create switches for all relays if enabled
    if config[CONF_AUTO_CREATE_SWITCHES]:
        max_relays = config[CONF_MODULE_COUNT] * 4
        name_prefix = config[CONF_NAME_PREFIX]

        for relay_id in range(max_relays):
            switch_config = {
                CONF_ID: cv.declare_id(SonoffSPMSwitch)(
                    f"sonoff_spm_switch_{relay_id}"
                ),
                "name": f"{name_prefix} {relay_id}",
                "relay_id": relay_id,
            }

            switch_var = cg.new_Pvariable(switch_config[CONF_ID])
            await cg.register_component(switch_var, {})
            await switch.register_switch(switch_var, switch_config)

            cg.add(switch_var.set_parent(var))
            cg.add(switch_var.set_relay_id(relay_id))
            cg.add(var.register_switch(switch_var, relay_id))

    # Auto-create sensors for all relays if enabled
    if config[CONF_AUTO_CREATE_SENSORS]:
        max_relays = config[CONF_MODULE_COUNT] * 4
        name_prefix = config[CONF_NAME_PREFIX]
        sensor_types = config[CONF_SENSOR_TYPES]

        for relay_id in range(max_relays):
            sensor_var = cg.new_Pvariable(
                cv.declare_id(SonoffSPMSensor)(f"sonoff_spm_sensor_{relay_id}")
            )
            await cg.register_component(sensor_var, {})

            cg.add(sensor_var.set_parent(var))
            cg.add(sensor_var.set_relay_id(relay_id))

            # Create requested sensor types
            if CONF_VOLTAGE in sensor_types:
                sens = await sensor.new_sensor(
                    {
                        "name": f"{name_prefix} {relay_id} Voltage",
                        "unit_of_measurement": UNIT_VOLT,
                        "accuracy_decimals": 1,
                        "device_class": DEVICE_CLASS_VOLTAGE,
                        "state_class": STATE_CLASS_MEASUREMENT,
                    }
                )
                cg.add(sensor_var.set_voltage_sensor(sens))

            if CONF_CURRENT in sensor_types:
                sens = await sensor.new_sensor(
                    {
                        "name": f"{name_prefix} {relay_id} Current",
                        "unit_of_measurement": UNIT_AMPERE,
                        "accuracy_decimals": 2,
                        "device_class": DEVICE_CLASS_CURRENT,
                        "state_class": STATE_CLASS_MEASUREMENT,
                    }
                )
                cg.add(sensor_var.set_current_sensor(sens))

            if CONF_POWER in sensor_types:
                sens = await sensor.new_sensor(
                    {
                        "name": f"{name_prefix} {relay_id} Power",
                        "unit_of_measurement": UNIT_WATT,
                        "accuracy_decimals": 1,
                        "device_class": DEVICE_CLASS_POWER,
                        "state_class": STATE_CLASS_MEASUREMENT,
                    }
                )
                cg.add(sensor_var.set_power_sensor(sens))

            if CONF_ENERGY in sensor_types:
                sens = await sensor.new_sensor(
                    {
                        "name": f"{name_prefix} {relay_id} Energy",
                        "unit_of_measurement": UNIT_KILOWATT_HOURS,
                        "accuracy_decimals": 2,
                        "device_class": DEVICE_CLASS_ENERGY,
                        "state_class": STATE_CLASS_TOTAL_INCREASING,
                    }
                )
                cg.add(sensor_var.set_energy_sensor(sens))

            cg.add(var.register_sensor(sensor_var, relay_id))
