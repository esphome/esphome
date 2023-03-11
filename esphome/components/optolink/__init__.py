from esphome import pins
import esphome.codegen as cg
from esphome.components import text_sensor as ts
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_LOGGER,
    CONF_PROTOCOL,
    CONF_RX_PIN,
    CONF_STATE,
    CONF_TX_PIN,
)
from esphome.core import CORE

CODEOWNERS = ["@j0ta29"]
DEPENDENCIES = []
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor", "number", "select", "switch"]
MULTI_CONF = False
CONF_DEVICE_INFO = "device_info"

optolink_ns = cg.esphome_ns.namespace("optolink")
OptolinkComponent = optolink_ns.class_("Optolink", cg.Component)
StateSensor = optolink_ns.class_(
    "OptolinkStateSensor", ts.TextSensor, cg.PollingComponent
)
STATE_SENSOR_ID = "state_sensor_id"
DeviceInfoSensor = optolink_ns.class_(
    "OptolinkDeviceInfoSensor", ts.TextSensor, cg.PollingComponent
)
DEVICE_INFO_SENSOR_ID = "device_info_sensor_id"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OptolinkComponent),
        cv.GenerateID(STATE_SENSOR_ID): cv.declare_id(StateSensor),
        cv.GenerateID(DEVICE_INFO_SENSOR_ID): cv.declare_id(DeviceInfoSensor),
        cv.Required(CONF_PROTOCOL): cv.one_of("P300", "KW"),
        cv.Optional(CONF_LOGGER, default=False): cv.boolean,
        cv.Optional(CONF_STATE): cv.string,
        cv.Optional(CONF_DEVICE_INFO): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)
if CORE.is_esp32:
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        cv.Schema(
            {
                cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_schema,
                cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_schema,
            }
        )
    )


async def to_code(config):
    cg.add_library("VitoWiFi", "1.0.2")

    cg.add_define(
        "VITOWIFI_PROTOCOL", cg.RawExpression(f"Optolink{config[CONF_PROTOCOL]}")
    )

    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_logger_enabled(config[CONF_LOGGER]))

    if CONF_STATE in config:
        debugSensor = cg.new_Pvariable(config[STATE_SENSOR_ID], config[CONF_STATE], var)
        await ts.register_text_sensor(
            debugSensor,
            {
                "id": config[STATE_SENSOR_ID],
                "name": config[CONF_STATE],
                "disabled_by_default": "false",
            },
        )
        await cg.register_component(debugSensor, config)

    if CONF_DEVICE_INFO in config:
        debugSensor = cg.new_Pvariable(
            config[DEVICE_INFO_SENSOR_ID], config[CONF_DEVICE_INFO], var
        )
        await ts.register_text_sensor(
            debugSensor,
            {
                "id": config[DEVICE_INFO_SENSOR_ID],
                "name": config[CONF_DEVICE_INFO],
                "disabled_by_default": "false",
            },
        )
        await cg.register_component(debugSensor, config)

    if CORE.is_esp32:
        cg.add(var.set_rx_pin(config[CONF_RX_PIN]["number"]))
        cg.add(var.set_tx_pin(config[CONF_TX_PIN]["number"]))

    await cg.register_component(var, config)
