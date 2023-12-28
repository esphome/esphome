import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = ["@vinhpn96"]
DEPENDENCIES = ["wifi"]

CONF_ON_READY = "on_ready"

smartconfig_ns = cg.esphome_ns.namespace("smartconfig")
SmartConfigComponent = smartconfig_ns.class_("SmartConfigComponent", cg.Component)
SmartConfigReadyTrigger = smartconfig_ns.class_(
    "SmartConfigReadyTrigger", automation.Trigger.template()
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SmartConfigComponent),
        cv.Optional(CONF_ON_READY): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SmartConfigReadyTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_SMARTCONFIG")

    for conf in config.get(CONF_ON_READY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
