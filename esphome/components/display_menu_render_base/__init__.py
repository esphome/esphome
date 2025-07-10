import esphome.codegen as cg
from esphome.components import display_menu_base, groups
from esphome.components.graphical_display_menu import GraphicalDisplayMenu
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY,
    CONF_BINARY_SENSOR,
    CONF_CUSTOM,
    CONF_GROUPS,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MENU_ID,
    CONF_SENSOR,
    CONF_SWITCH,
    CONF_TYPE,
)
from esphome.core.entity_types import ENTITY_TYPES
import esphome.cpp_types as core_types

display_menu_render_ns = cg.esphome_ns.namespace("display_menu_render_base")
BinarySensorMenuRender = display_menu_render_ns.class_("BinarySensorMenuRender")
SensorMenuRender = display_menu_render_ns.class_("SensorMenuRender")
SwitchMenuRender = display_menu_render_ns.class_("SwitchMenuRender")
LambdaMenuRender = display_menu_render_ns.class_("LambdaMenuRender")


CONF_ON_TEXT = "on_text"
CONF_OFF_TEXT = "off_text"
CONF_NO_DATA_TEXT = "no_data_text"

DISPLAY_MENU_RENDER_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MENU_ID): cv.use_id(GraphicalDisplayMenu),
    }
).extend(groups.LIST_OF_GROUPS_SCHEMA)

CONF_ENT_TYPE = "ent_type"

BASE_RENDER_SCHEMA = cv.typed_schema(
    {
        CONF_SWITCH: DISPLAY_MENU_RENDER_BASE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(SwitchMenuRender),
                cv.Optional(CONF_ON_TEXT, default="On"): cv.string_strict,
                cv.Optional(CONF_OFF_TEXT, default="Off"): cv.string_strict,
            }
        ),
        CONF_SENSOR: DISPLAY_MENU_RENDER_BASE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(SensorMenuRender),
                cv.Optional(CONF_NO_DATA_TEXT, default="Nan"): cv.string_strict,
                cv.Optional(CONF_ACCURACY, default=-1): cv.int_,
            }
        ),
        CONF_BINARY_SENSOR: DISPLAY_MENU_RENDER_BASE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(BinarySensorMenuRender),
                cv.Optional(CONF_ON_TEXT, default="On"): cv.string_strict,
                cv.Optional(CONF_OFF_TEXT, default="Off"): cv.string_strict,
                cv.Optional(CONF_NO_DATA_TEXT, default="Nan"): cv.string_strict,
            }
        ),
        CONF_CUSTOM: DISPLAY_MENU_RENDER_BASE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(LambdaMenuRender),
                cv.Required(CONF_LAMBDA): cv.lambda_,
                cv.Optional(CONF_ENT_TYPE): cv.enum(ENTITY_TYPES),
            }
        ),
    }
)

CONFIG_SCHEMA = cv.All(cv.ensure_list(BASE_RENDER_SCHEMA))


async def render_to_code(var, config):
    menu_var = await cg.get_variable(config[CONF_MENU_ID])
    cg.add(menu_var.add_render(var))

    if lambda_config := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lambda_config,
            [
                (display_menu_base.MenuItemMenuPtr, "menu"),
                (core_types.EntityBasePtr, "entity"),
            ],
            return_type=cg.size_t,
        )
        cg.add(var.set_lambda(lambda_))

    if ent_type := config.get(CONF_ENT_TYPE):
        cg.add(var.set_type(ent_type))

    if config.get(CONF_TYPE) == CONF_SWITCH:
        cg.add(var.set_on_text(config[CONF_ON_TEXT]))
        cg.add(var.set_off_text(config[CONF_OFF_TEXT]))

    if config.get(CONF_TYPE) == CONF_SENSOR:
        cg.add(var.set_no_data_text(config[CONF_NO_DATA_TEXT]))
        cg.add(var.set_accuracy(config[CONF_ACCURACY]))

    if config.get(CONF_TYPE) == CONF_BINARY_SENSOR:
        cg.add(var.set_on_text(config[CONF_ON_TEXT]))
        cg.add(var.set_off_text(config[CONF_OFF_TEXT]))
        cg.add(var.set_no_data_text(config[CONF_NO_DATA_TEXT]))

    if group_config := config.get(CONF_GROUPS):
        await groups.add_groups_to_storage(var, group_config)


async def to_code(config):
    for render in config:
        var = cg.new_Pvariable(render[CONF_ID])
        await render_to_code(var, render)
