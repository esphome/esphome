from esphome import automation
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_FORCE_UPDATE,
    CONF_ICON,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_WEB_SERVER,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_FIRMWARE,
    ENTITY_CATEGORY_CONFIG,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_generator import MockObjClass
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@jesserockz"]
IS_PLATFORM_COMPONENT = True

update_ns = cg.esphome_ns.namespace("update")
UpdateEntity = update_ns.class_("UpdateEntity", cg.EntityBase)

UpdateInfo = update_ns.struct("UpdateInfo")

PerformAction = update_ns.class_(
    "PerformAction", automation.Action, cg.Parented.template(UpdateEntity)
)
IsAvailableCondition = update_ns.class_(
    "IsAvailableCondition", automation.Condition, cg.Parented.template(UpdateEntity)
)

DEVICE_CLASSES = [
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_FIRMWARE,
]

CONF_ON_UPDATE_AVAILABLE = "on_update_available"

_UPDATE_SCHEMA = (
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


def update_schema(
    class_: MockObjClass = cv.UNDEFINED,
    *,
    icon: str = cv.UNDEFINED,
    device_class: str = cv.UNDEFINED,
    entity_category: str = cv.UNDEFINED,
) -> cv.Schema:
    schema = {}

    if class_ is not cv.UNDEFINED:
        schema[cv.GenerateID()] = cv.declare_id(class_)

    for key, default, validator in [
        (CONF_ICON, icon, cv.icon),
        (CONF_DEVICE_CLASS, device_class, cv.one_of(*DEVICE_CLASSES, lower=True)),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _UPDATE_SCHEMA.extend(schema)


# Remove before 2025.11.0
UPDATE_SCHEMA = update_schema()
UPDATE_SCHEMA.add_extra(cv.deprecated_schema_constant("update"))


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

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)


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


@automation.register_action(
    "update.perform",
    PerformAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(UpdateEntity),
            cv.Optional(CONF_FORCE_UPDATE, default=False): cv.templatable(cv.boolean),
        }
    ),
)
async def update_perform_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    force = await cg.templatable(config[CONF_FORCE_UPDATE], args, cg.bool_)
    cg.add(var.set_force(force))
    return var


@automation.register_condition(
    "update.is_available",
    IsAvailableCondition,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(UpdateEntity),
        }
    ),
)
async def update_is_available_condition_to_code(
    config, condition_id, template_arg, args
):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
