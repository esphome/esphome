import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_FLOW,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    DEVICE_CLASS_WATER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_CUBIC_METER_PER_HOUR,
    UNIT_LITRE,
)

CODEOWNERS = ["@ljungqvist"]

MULTI_CONF = True

AUTO_LOAD = ["sensor"]

DEPENDENCIES = ["uart"]

CONF_ACCUMULATED_FLOW = "accumulated_flow"
ufm01_ns = cg.esphome_ns.namespace("ufm01")
UFM01Component = ufm01_ns.class_("UFM01Component", uart.UARTDevice, cg.Component)

CONF_UFM01_ID = "ufm01_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UFM01Component),
            cv.Optional(CONF_ACCUMULATED_FLOW): sensor.sensor_schema(
                unit_of_measurement=UNIT_LITRE,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_WATER,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_FLOW): sensor.sensor_schema(
                unit_of_measurement=UNIT_CUBIC_METER_PER_HOUR,
                accuracy_decimals=5,
                device_class=DEVICE_CLASS_VOLUME_FLOW_RATE,
                state_class=STATE_CLASS_MEASUREMENT,
                icon="mdi:waves-arrow-right",
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                icon="mdi:thermometer-water",
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ufm01",
    require_tx=True,
    require_rx=True,
    baud_rate=2400,
    parity="EVEN",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_ACCUMULATED_FLOW in config:
        sens = await sensor.new_sensor(config[CONF_ACCUMULATED_FLOW])
        cg.add(var.set_volume_sensor(sens))

    if CONF_FLOW in config:
        sens = await sensor.new_sensor(config[CONF_FLOW])
        cg.add(var.set_flow_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
