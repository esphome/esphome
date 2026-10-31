import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from . import CONF_DFROBOT_SEN0395_ID, DfrobotSen0395Component, dfrobot_sen0395_ns

DEPENDENCIES = ["dfrobot_sen0395"]

CommandType = dfrobot_sen0395_ns.enum("CommandType", is_class=True)

_SWITCH_SCHEMA = switch.switch_schema(
    switch.ApplySwitch,
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(
    {
        cv.GenerateID(CONF_DFROBOT_SEN0395_ID): cv.use_id(DfrobotSen0395Component),
    }
)

# Per type: the hub setter that stores the switch, and the command its writes queue
_SWITCHES = {
    "sensor_active": (
        lambda hub: hub.set_sensor_active_switch,
        f"enqueue({CommandType.COMMAND_TYPE_POWER}, state)",
    ),
    "turn_on_led": (
        lambda hub: hub.set_turn_on_led_switch,
        f"enqueue_saved_setting({CommandType.COMMAND_TYPE_LED_MODE}, state)",
    ),
    "presence_via_uart": (
        lambda hub: hub.set_presence_via_uart_switch,
        f"enqueue_saved_setting({CommandType.COMMAND_TYPE_UART_OUTPUT}, state)",
    ),
    "start_after_boot": (
        lambda hub: hub.set_start_after_boot_switch,
        f"enqueue_saved_setting({CommandType.COMMAND_TYPE_SENSOR_CFG_START}, state)",
    ),
}

CONFIG_SCHEMA = cv.typed_schema(dict.fromkeys(_SWITCHES, _SWITCH_SCHEMA))


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_SEN0395_ID])
    setter, apply = _SWITCHES[config[CONF_TYPE]]
    await switch.sub_switches(config, parent=hub).create(
        config, setter(hub), apply=apply
    )
