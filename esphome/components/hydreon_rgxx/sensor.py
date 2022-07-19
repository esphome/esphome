import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    CONF_MOISTURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    ICON_THERMOMETER,
)

from . import RGModel, HydreonRGxxComponent

UNIT_INTENSITY = "intensity"
UNIT_MILLIMETERS = "mm"
UNIT_MILLIMETERS_PER_HOUR = "mm/h"

CONF_ACC = "acc"
CONF_EVENT_ACC = "event_acc"
CONF_TOTAL_ACC = "total_acc"
CONF_R_INT = "r_int"

RG_MODELS = {
    "RG_9": RGModel.RG9,
    "RG_15": RGModel.RG15,
    # https://rainsensors.com/wp-content/uploads/sites/3/2020/07/rg-15_instructions_sw_1.000.pdf
    # https://rainsensors.com/wp-content/uploads/sites/3/2021/03/2020.08.25-rg-9_instructions.pdf
    # https://rainsensors.com/wp-content/uploads/sites/3/2021/03/2021.03.11-rg-9_instructions.pdf
}
SUPPORTED_SENSORS = {
    CONF_ACC: ["RG_15"],
    CONF_EVENT_ACC: ["RG_15"],
    CONF_TOTAL_ACC: ["RG_15"],
    CONF_R_INT: ["RG_15"],
    CONF_MOISTURE: ["RG_9"],
    CONF_TEMPERATURE: ["RG_9"],
}
PROTOCOL_NAMES = {
    CONF_MOISTURE: "R",
    CONF_ACC: "Acc",
    CONF_R_INT: "RInt",
    CONF_EVENT_ACC: "EventAcc",
    CONF_TOTAL_ACC: "TotalAcc",
    CONF_TEMPERATURE: "t",
}


def _validate(config):
    for conf, models in SUPPORTED_SENSORS.items():
        if conf in config:
            if config[CONF_MODEL] not in models:
                raise cv.Invalid(
                    f"{conf} is only available on {' and '.join(models)}, not {config[CONF_MODEL]}"
                )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HydreonRGxxComponent),
            cv.Required(CONF_MODEL): cv.enum(
                RG_MODELS,
                upper=True,
                space="_",
            ),
            cv.Optional(CONF_ACC): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIMETERS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_EVENT_ACC): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIMETERS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TOTAL_ACC): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIMETERS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_R_INT): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIMETERS_PER_HOUR,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MOISTURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_INTENSITY,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                icon=ICON_THERMOMETER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add_define(
        "HYDREON_RGXX_PROTOCOL_LIST(F, sep)",
        cg.RawExpression(
            " sep ".join([f'F("{name} ")' for name in PROTOCOL_NAMES.values()])
        ),
    )
    cg.add_define("HYDREON_RGXX_NUM_SENSORS", len(PROTOCOL_NAMES))

    for i, conf in enumerate(PROTOCOL_NAMES):
        if conf in config:
            sens = await sensor.new_sensor(config[conf])
            cg.add(var.set_sensor(sens, i))

    cg.add(var.set_request_temperature(CONF_TEMPERATURE in config))
