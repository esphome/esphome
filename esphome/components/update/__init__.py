from esphome import automation
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_WEB_SERVER_ID,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_FIRMWARE,
    ENTITY_CATEGORY_CONFIG,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@jesserockz"]
IS_PLATFORM_COMPONENT = True

update_ns = cg.esphome_ns.namespace("update")
UpdateEntity = update_ns.class_("UpdateEntity", cg.EntityBase)

UpdateInfo = update_ns.struct("UpdateInfo")

PerformAction = update_ns.class_("PerformAction", automation.Action)
IsAvailableCondition = update_ns.class_("IsAvailableCondition", automation.Condition)

DEVICE_CLASSES = [
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_FIRMWARE,
]

CONF_ON_UPDATE_AVAILABLE = "on_update_available"

UPDATE_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTUpdateComponent),
            cv.Optional(CONF_DEVICE_CLASS): cv.one_of(*DEVICE_CLASSES, lower=True),
            cv.Optional(CONF_ON_UPDATE_AVAILABLE): automation.validate_automation(
                single=True
            ),
            cv.Optional(
                CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
            ): cv.entity_category,
        }
    )
)


async def setup_update_core_(var, config):
    await setup_entity(var, config)

    if device_class_config := config.get(CONF_DEVICE_CLASS):
        cg.add(var.set_device_class(device_class_config))

    if on_update_available := config.get(CONF_ON_UPDATE_AVAILABLE):
        await automation.build_automation(
            var.get_update_available_trigger(),
            [(UpdateInfo.operator("ref").operator("const"), "x")],
            on_update_available,
        )

    if mqtt_id_config := config.get(CONF_MQTT_ID):
        mqtt_ = cg.new_Pvariable(mqtt_id_config, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if web_server_id_config := config.get(CONF_WEB_SERVER_ID):
        web_server_ = await cg.get_variable(web_server_id_config)
        web_server.add_entity_to_sorting_list(web_server_, var, config)


async def register_update(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_update(var))
    await setup_update_core_(var, config)


async def new_update(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_update(var, config)
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_UPDATE")
    cg.add_global(update_ns.using)


UPDATE_AUTOMATION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(UpdateEntity),
    }
)


@automation.register_action("update.perform", PerformAction, UPDATE_AUTOMATION_SCHEMA)
async def update_perform_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, paren, paren)


@automation.register_condition(
    "update.is_available", IsAvailableCondition, UPDATE_AUTOMATION_SCHEMA
)
async def update_is_available_condition_to_code(
    config, condition_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, paren, paren)
