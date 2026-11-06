from esphome.components import event
import esphome.config_validation as cv
from esphome.const import CONF_DEVICE_CLASS, CONF_EVENT_TYPES, CONF_VARIABLES
from esphome.types import ConfigType

from .. import template_ns

CODEOWNERS = ["@nohat"]

TemplateEvent = template_ns.class_("TemplateEvent", event.Event)


def _check_variables(config: ConfigType) -> ConfigType:
    event.check_variables(config.get(CONF_VARIABLES, {}), config[CONF_EVENT_TYPES])
    return config


CONFIG_SCHEMA = cv.All(
    cv.with_visibility(
        event.event_schema(TemplateEvent), cv.Visibility.UI, CONF_DEVICE_CLASS
    ).extend(
        {
            cv.Required(CONF_EVENT_TYPES): cv.ensure_list(cv.string_strict),
            cv.Optional(CONF_VARIABLES): event.validate_variables,
        }
    ),
    _check_variables,
)


async def to_code(config: ConfigType) -> None:
    await event.new_event(
        config,
        event_types=config[CONF_EVENT_TYPES],
        variables=config.get(CONF_VARIABLES),
    )
