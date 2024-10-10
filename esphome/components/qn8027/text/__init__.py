from typing import Optional
import esphome.codegen as cg
from esphome.components import text
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_MQTT_ID,
    CONF_WEB_SERVER,
    ENTITY_CATEGORY_CONFIG,
)
from esphome.core import CORE
from esphome.cpp_generator import MockObjClass
from esphome.cpp_helpers import setup_entity
from .. import (
    CONF_QN8027_ID,
    QN8027Component,
    qn8027_ns,
    CONF_RDS_STATION,
    CONF_RDS_TEXT,
    ICON_FORMAT_TEXT,
)

RDSStationText = qn8027_ns.class_("RDSStationText", text.Text)
RDSTextText = qn8027_ns.class_("RDSTextText", text.Text)

# The text component isn't implemented the same way as switch, select, number.
# It is not possible to create our own text platform as it is now, so I adopted
# the necessary code from those.

_TEXT_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTTextComponent),
        }
    )
)

_UNDEF = object()


def text_schema(
    class_: MockObjClass = _UNDEF,
    *,
    entity_category: str = _UNDEF,
    device_class: str = _UNDEF,
    icon: str = _UNDEF,
):
    schema = _TEXT_SCHEMA
    if class_ is not _UNDEF:
        schema = schema.extend({cv.GenerateID(): cv.declare_id(class_)})
    if entity_category is not _UNDEF:
        schema = schema.extend(
            {
                cv.Optional(
                    CONF_ENTITY_CATEGORY, default=entity_category
                ): cv.entity_category
            }
        )
    if icon is not _UNDEF:
        schema = schema.extend({cv.Optional(CONF_ICON, default=icon): cv.icon})
    return schema


TEXT_SCHEMA = text_schema()  # for compatibility


async def setup_text_core_(
    var,
    config,
    *,
    min_length: Optional[int],
    max_length: Optional[int],
    pattern: Optional[str],
):
    await setup_entity(var, config)
    cg.add(var.traits.set_min_length(min_length))
    cg.add(var.traits.set_max_length(max_length))
    if pattern is not None:
        cg.add(var.traits.set_pattern(pattern))

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)


async def register_text(
    var,
    config,
    min_length: Optional[int] = 0,
    max_length: Optional[int] = 255,
    pattern: Optional[str] = None,
):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_text(var))
    await setup_text_core_(
        var, config, min_length=min_length, max_length=max_length, pattern=pattern
    )


async def new_text(
    config,
    *args,
    min_length: Optional[int] = 0,
    max_length: Optional[int] = 255,
    pattern: Optional[str] = None,
):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_text(
        var, config, min_length=min_length, max_length=max_length, pattern=pattern
    )
    return var


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_QN8027_ID): cv.use_id(QN8027Component),
        cv.Optional(CONF_RDS_STATION): text_schema(
            RDSStationText,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_FORMAT_TEXT,
        ),
        cv.Optional(CONF_RDS_TEXT): text_schema(
            RDSTextText,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_FORMAT_TEXT,
        ),
    }
)


async def to_code(config):
    qn8027_component = await cg.get_variable(config[CONF_QN8027_ID])
    if rds_station_config := config.get(CONF_RDS_STATION):
        t = await new_text(
            rds_station_config, min_length=0, max_length=qn8027_ns.RDS_STATION_MAX_SIZE
        )
        await cg.register_parented(t, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_rds_station_text(t))
    if rds_text_config := config.get(CONF_RDS_TEXT):
        t = await new_text(
            rds_text_config, min_length=0, max_length=qn8027_ns.RDS_TEXT_MAX_SIZE
        )
        await cg.register_parented(t, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_rds_text_text(t))
