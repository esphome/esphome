import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, ICON_NEW_BOX

from . import (
    CONF_DBUS_SYSTEM,
    CONF_DBUS_DESTINATION,
    CONF_DBUS_PATH,
    CONF_DBUS_INTERFACE,
    CONF_DBUS_METHOD,
    CONF_DBUS_ARGS,
)

DEPENDENCIES = ["dbus"]

CONF_PROPERTIES = "properties"
CONF_PROPERTY_SEPARATOR = "property_separator"

dbus_ns = cg.esphome_ns.namespace("dbus")
# DBus = dbus_ns.class_("DBus", cg.Component)


DBusTextSensor = dbus_ns.class_("DBusTextSensor", text_sensor.TextSensor, cg.Component)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(
        icon=ICON_NEW_BOX,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(DBusTextSensor),
            cv.Optional(CONF_DBUS_SYSTEM, default=False): cv.boolean,
            cv.Required(CONF_DBUS_DESTINATION): cv.string,
            cv.Required(CONF_DBUS_PATH): cv.string,
            cv.Required(CONF_DBUS_INTERFACE): cv.string,
            cv.Optional(CONF_DBUS_METHOD, default="Get"): cv.string,
            cv.Optional(CONF_DBUS_ARGS, default=[]): cv.ensure_list(cv.string),
            # workaround: properties: ["root"] to return the root value
            cv.Optional(CONF_PROPERTIES, default=[]): cv.ensure_list(cv.string),
            cv.Optional(CONF_PROPERTY_SEPARATOR, default=", "): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_system(config[CONF_DBUS_SYSTEM]))
    cg.add(var.set_dbus_destination(config[CONF_DBUS_DESTINATION]))
    cg.add(var.set_dbus_path(config[CONF_DBUS_PATH]))
    cg.add(var.set_dbus_interface(config[CONF_DBUS_INTERFACE]))
    cg.add(var.set_dbus_method(config[CONF_DBUS_METHOD]))
    cg.add(var.set_dbus_args(config[CONF_DBUS_ARGS]))
    cg.add(var.set_properties(config[CONF_PROPERTIES]))
    cg.add(var.set_property_separator(config[CONF_PROPERTY_SEPARATOR]))
