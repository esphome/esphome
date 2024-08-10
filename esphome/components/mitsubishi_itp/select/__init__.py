import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (
    select,
)
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_NONE,
)
from esphome.core import coroutine
from ...mitsubishi_itp import (
    CONF_MITSUBISHI_ITP_ID,
    mitsubishi_itp_ns,
    MitsubishiUART,
)

CONF_TEMPERATURE_SOURCE = (
    "temperature_source"  # This is to create a Select object for selecting a source
)
CONF_VANE_POSITION = "vane_position"
CONF_HORIZONTAL_VANE_POSITION = "horizontal_vane_position"

VANE_POSITIONS = ["Auto", "1", "2", "3", "4", "5", "Swing"]
HORIZONTAL_VANE_POSITIONS = ["Auto", "<<", "<", "|", ">", ">>", "<>", "Swing"]

TemperatureSourceSelect = mitsubishi_itp_ns.class_(
    "TemperatureSourceSelect", select.Select
)
VanePositionSelect = mitsubishi_itp_ns.class_("VanePositionSelect", select.Select)
HorizontalVanePositionSelect = mitsubishi_itp_ns.class_(
    "HorizontalVanePositionSelect", select.Select
)

SELECTS = {
    CONF_TEMPERATURE_SOURCE: (
        select.select_schema(
            TemperatureSourceSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:thermometer-check",
        ),
        [mitsubishi_itp_ns.TEMPERATURE_SOURCE_INTERNAL],
    ),
    CONF_VANE_POSITION: (
        select.select_schema(
            VanePositionSelect,
            entity_category=ENTITY_CATEGORY_NONE,
            icon="mdi:arrow-expand-vertical",
        ),
        VANE_POSITIONS,
    ),
    CONF_HORIZONTAL_VANE_POSITION: (
        select.select_schema(
            HorizontalVanePositionSelect,
            entity_category=ENTITY_CATEGORY_NONE,
            icon="mdi:arrow-expand-horizontal",
        ),
        HORIZONTAL_VANE_POSITIONS,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MITSUBISHI_ITP_ID): cv.use_id(MitsubishiUART),
    }
).extend(
    {
        cv.Optional(select_designator): select_schema
        for select_designator, (
            select_schema,
            _,
        ) in SELECTS.items()
    }
)


@coroutine
async def to_code(config):
    muart_component = await cg.get_variable(config[CONF_MITSUBISHI_ITP_ID])

    # Register selects
    for select_designator, (
        _,
        select_options,
    ) in SELECTS.items():
        if select_conf := config.get(select_designator):
            select_component = cg.new_Pvariable(select_conf[CONF_ID])
            cg.add(
                getattr(muart_component, f"set_{select_designator}_select")(
                    select_component
                )
            )
            await cg.register_parented(select_component, muart_component)

            await select.register_select(
                select_component, select_conf, options=select_options
            )
