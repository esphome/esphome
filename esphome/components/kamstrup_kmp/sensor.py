import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMMAND,
    CONF_CUSTOM,
    CONF_FLOW,
    CONF_ID,
    CONF_POWER,
    CONF_VOLUME,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLUME,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_CUBIC_METER,
    UNIT_EMPTY,
    UNIT_KELVIN,
    UNIT_KILOWATT,
)

CODEOWNERS = ["@cfeenstra1024"]
DEPENDENCIES = ["uart"]

kamstrup_kmp_ns = cg.esphome_ns.namespace("kamstrup_kmp")
KamstrupKMPComponent = kamstrup_kmp_ns.class_(
    "KamstrupKMPComponent", cg.PollingComponent, uart.UARTDevice
)

CONF_HEAT_ENERGY = "heat_energy"
CONF_TEMP1 = "temp1"
CONF_TEMP2 = "temp2"
CONF_TEMP_DIFF = "temp_diff"

UNIT_GIGA_JOULE = "GJ"
UNIT_LITRE_PER_HOUR = "l/h"

# Note: The sensor units are set automatically based un the received data from the meter
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(KamstrupKMPComponent),
            cv.Optional(CONF_HEAT_ENERGY): sensor.sensor_schema(
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                unit_of_measurement=UNIT_GIGA_JOULE,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement=UNIT_KILOWATT,
            ),
            cv.Optional(CONF_TEMP1): sensor.sensor_schema(
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement=UNIT_CELSIUS,
            ),
            cv.Optional(CONF_TEMP2): sensor.sensor_schema(
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement=UNIT_CELSIUS,
            ),
            cv.Optional(CONF_TEMP_DIFF): sensor.sensor_schema(
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement=UNIT_KELVIN,
            ),
            cv.Optional(CONF_FLOW): sensor.sensor_schema(
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLUME,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement=UNIT_LITRE_PER_HOUR,
            ),
            cv.Optional(CONF_VOLUME): sensor.sensor_schema(
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLUME,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                unit_of_measurement=UNIT_CUBIC_METER,
            ),
            cv.Optional(CONF_CUSTOM): cv.ensure_list(
                sensor.sensor_schema(
                    accuracy_decimals=1,
                    device_class=DEVICE_CLASS_EMPTY,
                    state_class=STATE_CLASS_MEASUREMENT,
                    unit_of_measurement=UNIT_EMPTY,
                ).extend({cv.Required(CONF_COMMAND): cv.hex_uint16_t})
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "kamstrup_kmp", baud_rate=1200, require_rx=True, require_tx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Standard sensors
    for key in [
        CONF_HEAT_ENERGY,
        CONF_POWER,
        CONF_TEMP1,
        CONF_TEMP2,
        CONF_TEMP_DIFF,
        CONF_FLOW,
        CONF_VOLUME,
    ]:
        if key not in config:
            continue
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(var, f"set_{key}_sensor")(sens))

    # Custom sensors
    if CONF_CUSTOM in config:
        for conf in config[CONF_CUSTOM]:
            sens = await sensor.new_sensor(conf)
            cg.add(var.add_custom_sensor(sens, conf[CONF_COMMAND]))
