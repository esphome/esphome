import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.types import ConfigType

from .cover import Tormatic

CONF_TORMATIC_ID = "tormatic_id"

CONFIG_SCHEMA = switch.switch_schema(switch.ApplySwitch).extend(
    {
        cv.Required(CONF_TORMATIC_ID): cv.use_id(Tormatic),
    }
)


async def to_code(config: ConfigType) -> None:
    paren = await cg.get_variable(config[CONF_TORMATIC_ID])
    switches = switch.sub_switches(config, parent=paren)
    await switches.create(
        config,
        paren.set_light_switch,
        apply="send_light_command(state)",
        publish="after",
    )
