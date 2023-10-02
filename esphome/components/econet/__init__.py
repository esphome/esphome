import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart
from esphome.const import CONF_ID, CONF_MODEL, CONF_SENSOR_DATAPOINT, CONF_TRIGGER_ID

DEPENDENCIES = ["uart"]

CONF_ON_DATAPOINT_UPDATE = "on_datapoint_update"
CONF_DATAPOINT_TYPE = "datapoint_type"
CONF_REQUEST_MOD = "request_mod"
MAX_REQUEST_MOD = 7

econet_ns = cg.esphome_ns.namespace("econet")
Econet = econet_ns.class_("Econet", cg.Component, uart.UARTDevice)
EconetClient = econet_ns.class_("EconetClient")

DPTYPE_RAW = "raw"

DATAPOINT_TYPES = {
    DPTYPE_RAW: cg.std_vector.template(cg.uint8),
}

DATAPOINT_TRIGGERS = {
    DPTYPE_RAW: econet_ns.class_(
        "EconetRawDatapointUpdateTrigger",
        automation.Trigger.template(DATAPOINT_TYPES[DPTYPE_RAW]),
    ),
}


def assign_declare_id(value):
    value = value.copy()
    value[CONF_TRIGGER_ID] = cv.declare_id(
        DATAPOINT_TRIGGERS[value[CONF_DATAPOINT_TYPE]]
    )(value[CONF_TRIGGER_ID].id)
    return value


ModelType = econet_ns.enum("ModelType")
MODEL_TYPES = {
    "Tankless": ModelType.MODEL_TYPE_TANKLESS,
    "Heatpump": ModelType.MODEL_TYPE_HEATPUMP,
    "HVAC": ModelType.MODEL_TYPE_HVAC,
    "Electric Tank": ModelType.MODEL_TYPE_ELECTRIC_TANK,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Econet),
            cv.Required(CONF_MODEL): cv.enum(MODEL_TYPES),
            cv.Optional(CONF_ON_DATAPOINT_UPDATE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        DATAPOINT_TRIGGERS[DPTYPE_RAW]
                    ),
                    cv.Required(CONF_SENSOR_DATAPOINT): cv.string,
                    cv.Optional(CONF_REQUEST_MOD, default=-1): cv.int_range(
                        min=-1, max=MAX_REQUEST_MOD
                    ),
                    cv.Optional(CONF_DATAPOINT_TYPE, default=DPTYPE_RAW): cv.one_of(
                        *DATAPOINT_TRIGGERS, lower=True
                    ),
                },
                extra_validators=assign_declare_id,
            ),
        }
    )
    .extend(cv.polling_component_schema("30s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

CONF_ECONET_ID = "econet_id"
ECONET_CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ECONET_ID): cv.use_id(Econet),
        cv.Optional(CONF_REQUEST_MOD, default=0): cv.int_range(
            min=-1, max=MAX_REQUEST_MOD
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_model_type(config[CONF_MODEL]))
    for conf in config.get(CONF_ON_DATAPOINT_UPDATE, []):
        trigger = cg.new_Pvariable(
            conf[CONF_TRIGGER_ID],
            var,
            conf[CONF_SENSOR_DATAPOINT],
            conf[CONF_REQUEST_MOD],
        )
        await automation.build_automation(
            trigger, [(DATAPOINT_TYPES[conf[CONF_DATAPOINT_TYPE]], "x")], conf
        )
