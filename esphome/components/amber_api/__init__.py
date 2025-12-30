from esphome import automation
import esphome.codegen as cg
from esphome.components.http_request import HttpRequestComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_UPDATE, CONF_TRIGGER_ID

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["http_request", "json"]
AUTO_LOAD = ["http_request", "json"]

CONF_AMBER_API_ID = "amber_api_id"
CONF_API_KEY = "api_key"
CONF_SITE_ID = "site_id"
CONF_HTTP_REQUEST_ID = "http_request_id"

amber_api_ns = cg.esphome_ns.namespace("amber_api")
AmberApiComponent = amber_api_ns.class_("AmberApiComponent", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AmberApiComponent),
        cv.Required(CONF_API_KEY): cv.string,
        cv.Required(CONF_SITE_ID): cv.string,
        cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
        cv.Optional(CONF_ON_UPDATE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    automation.Trigger.template()
                ),
            }
        ),
    }
).extend(cv.polling_component_schema("5min"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_api_key(config[CONF_API_KEY]))
    cg.add(var.set_site_id(config[CONF_SITE_ID]))

    http_request = await cg.get_variable(config[CONF_HTTP_REQUEST_ID])
    cg.add(var.set_http_request(http_request))

    for conf in config.get(CONF_ON_UPDATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await automation.build_automation(trigger, [], conf)
        cg.add(var.add_on_update_trigger(trigger))
