import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_EFFECTS,
    CONF_FLASH_TRANSITION_LENGTH,
    CONF_GAMMA_CORRECT,
    CONF_INITIAL_STATE,
    CONF_RESTORE_MODE,
    CONF_RESTORE_STATE,
)
from esphome.types import ConfigType

from .. import (
    HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA,
    final_validate_entity_id,
    homeassistant_ns,
    setup_home_assistant_entity,
    validate_entity_domain,
)

CODEOWNERS = ["@egormanga"]
DEPENDENCIES = ["api"]

HomeassistantLight = homeassistant_ns.class_(
    "HomeassistantLight", light.LightOutput, cg.Component
)

_RENDERED_BY_HOME_ASSISTANT = cv.invalid(
    "Home Assistant renders this light and keeps its state; this option has no effect here"
)

CONFIG_SCHEMA = cv.All(
    light.light_schema(HomeassistantLight, light.LightType.RGB)
    .extend(HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA)
    .extend(
        {
            # A transition would be stepped over the API; the target is sent once instead
            cv.Optional(
                CONF_DEFAULT_TRANSITION_LENGTH, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_GAMMA_CORRECT): _RENDERED_BY_HOME_ASSISTANT,
            cv.Optional(CONF_EFFECTS): _RENDERED_BY_HOME_ASSISTANT,
            cv.Optional(CONF_FLASH_TRANSITION_LENGTH): _RENDERED_BY_HOME_ASSISTANT,
            cv.Optional(
                light.CONF_TRANSITION_STATE_PUBLISH_INTERVAL
            ): _RENDERED_BY_HOME_ASSISTANT,
            cv.Optional(CONF_RESTORE_MODE): _RENDERED_BY_HOME_ASSISTANT,
            cv.Optional(CONF_RESTORE_STATE): _RENDERED_BY_HOME_ASSISTANT,
            cv.Optional(CONF_INITIAL_STATE): _RENDERED_BY_HOME_ASSISTANT,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_entity_domain("light", ("light",)),
)

FINAL_VALIDATE_SCHEMA = final_validate_entity_id


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_API_HOMEASSISTANT_SERVICES")
    var = await light.new_light(config)
    await cg.register_component(var, config)
    setup_home_assistant_entity(var, config)
