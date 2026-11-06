from esphome.automation import apply_parent
from esphome.components import button, output
import esphome.config_validation as cv
from esphome.const import CONF_DURATION, CONF_OUTPUT
from esphome.types import ConfigType

CONFIG_SCHEMA = button.button_schema(button.ApplyButton).extend(
    {
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        cv.Required(CONF_DURATION): cv.positive_time_period_milliseconds,
    }
)


async def to_code(config: ConfigType) -> None:
    out = await apply_parent(config, CONF_OUTPUT)
    duration = config[CONF_DURATION].total_milliseconds
    await button.sub_buttons(config).create(
        config,
        None,
        apply=f"output::press_output_button(entity, {out}, {duration})",
    )
