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
AUTO_LOAD = ["homeassistant"]

SUPPORTED_DOMAINS = ["light"]

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
            **{
                cv.Optional(key): _RENDERED_BY_HOME_ASSISTANT
                for key in (
                    CONF_GAMMA_CORRECT,
                    CONF_EFFECTS,
                    CONF_FLASH_TRANSITION_LENGTH,
                    light.CONF_TRANSITION_STATE_PUBLISH_INTERVAL,
                    CONF_RESTORE_MODE,
                    CONF_RESTORE_STATE,
                    CONF_INITIAL_STATE,
                )
            },
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_entity_domain("light", SUPPORTED_DOMAINS),
)

FINAL_VALIDATE_SCHEMA = final_validate_entity_id


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_API_HOMEASSISTANT_SERVICES")
    var = await light.new_light(config)
    await cg.register_component(var, config)
    setup_home_assistant_entity(var, config)
