import esphome.config_validation as cv
from esphome.components import select
from esphome.components.jablotron import (
    ACCESS_CODE_SCHEMA,
    INDEX_SCHEMA,
    JABLOTRON_DEVICE_SCHEMA,
    set_access_code,
    set_index,
    register_jablotron_device,
)
from esphome.components.jablotron_section import jablotron_section_ns

SectionSelect = jablotron_section_ns.class_("SectionSelect", select.Select)

AUTO_LOAD = ["select"]
DEPENDENCIES = ["jablotron"]
CONFIG_SCHEMA = (
    select.SELECT_SCHEMA.extend(JABLOTRON_DEVICE_SCHEMA)
    .extend(INDEX_SCHEMA)
    .extend(ACCESS_CODE_SCHEMA)
    .extend({cv.GenerateID(): cv.declare_id(SectionSelect)})
)


async def to_code(config):
    sel = await select.new_select(config, options=["READY", "ARMED_PART", "ARMED"])
    set_index(sel, config)
    set_access_code(sel, config)
    await register_jablotron_device(sel, config)
