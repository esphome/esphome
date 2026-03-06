from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_UPDATE, CONF_TRIGGER_ID

CODEOWNERS = ["@oarcher"]
DEPENDENCIES = ["uart"]

IS_PLATFORM_COMPONENT = True

CONF_NMEA_ID = "nmea_id"

CODEOWNERS = ["@oarcher"]

nmea_ns = cg.esphome_ns.namespace("nmea")
NMEAComponent = nmea_ns.class_("NMEAComponent", uart.UARTComponent, cg.PollingComponent)
NMEAOnUpdateTrigger = nmea_ns.class_(
    "NMEAOnUpdateTrigger", automation.Trigger.template()
)

NMEA_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NMEAComponent),
            cv.Optional(CONF_ON_UPDATE): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NMEAOnUpdateTrigger)}
            ),
        }
    )
    .extend(cv.polling_component_schema("20s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def new_nmea(config):
    """Helper function for platforms to create NMEA components."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    for conf in config.get(CONF_ON_UPDATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    return var
