import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLUME,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL,
)

CODEOWNERS = ["@cfeenstra1024"]
DEPENDENCIES = ["uart"]

kamstrup_mc40x_ns = cg.esphome_ns.namespace("kamstrup_mc40x")
KamstrupMC40xComponent = kamstrup_mc40x_ns.class_(
    "KamstrupMC40xComponent", cg.PollingComponent, uart.UARTDevice
)

CONF_KAMSTRUP_HEAT_ENERGY = "heat_energy"
CONF_KAMSTRUP_POWER = "power"
CONF_KAMSTRUP_TEMP1 = "temp1"
CONF_KAMSTRUP_TEMP2 = "temp2"
CONF_KAMSTRUP_TEMP_DIFF = "temp_diff"
CONF_KAMSTRUP_FLOW = "flow"
CONF_KAMSTRUP_VOLUME = "volume"

# Note: The sensor units are set automatically based un the received data from the meter
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(KamstrupMC40xComponent),
            cv.Optional(CONF_KAMSTRUP_HEAT_ENERGY): sensor.sensor_schema(
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL,
            ),
            cv.Optional(CONF_KAMSTRUP_POWER): sensor.sensor_schema(
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_KAMSTRUP_TEMP1): sensor.sensor_schema(
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_KAMSTRUP_TEMP2): sensor.sensor_schema(
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_KAMSTRUP_TEMP_DIFF): sensor.sensor_schema(
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_KAMSTRUP_FLOW): sensor.sensor_schema(
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLUME,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_KAMSTRUP_VOLUME): sensor.sensor_schema(
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLUME,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "kamstrup_mc40x", baud_rate=1200, require_rx=True, require_tx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    for key in [
        CONF_KAMSTRUP_HEAT_ENERGY,
        CONF_KAMSTRUP_POWER,
        CONF_KAMSTRUP_TEMP1,
        CONF_KAMSTRUP_TEMP2,
        CONF_KAMSTRUP_TEMP_DIFF,
        CONF_KAMSTRUP_FLOW,
        CONF_KAMSTRUP_VOLUME,
    ]:
        if key not in config:
            continue
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(var, f"set_{key}_sensor")(sens))
