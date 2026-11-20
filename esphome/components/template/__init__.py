import esphome.codegen as cg
from esphome.components.api import wizard
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DOMAIN = "template"

CONF_WIZARD_ROLE = "wizard_role"

template_ns = cg.esphome_ns.namespace("template_")

_WIZARD_ROLE_NAME_MAX_LENGTH = wizard.WIZARD_ROLE_MAX_LENGTH - len(DOMAIN) - 1


def _wizard_role_name(value: str) -> str:
    """Validate the name of a wizard role; the role itself is template.<name>."""
    if (
        not wizard.WIZARD_ROLE_NAME_PATTERN.fullmatch(value)
        or len(value) > _WIZARD_ROLE_NAME_MAX_LENGTH
    ):
        raise cv.Invalid(
            f"A wizard role is 1 to {_WIZARD_ROLE_NAME_MAX_LENGTH} lowercase letters, "
            "digits and underscores"
        )
    return value


WIZARD_ROLE_SCHEMA = cv.Schema(
    {cv.Optional(CONF_WIZARD_ROLE): cv.All(cv.string_strict, _wizard_role_name)}
)


def register_wizard_role(config: ConfigType) -> ConfigType:
    """Give the entity the wizard role template.<wizard_role>, when it sets one."""
    if (name := config.get(CONF_WIZARD_ROLE)) is not None:
        wizard.register_entity_role(config[CONF_ID], f"{DOMAIN}.{name}")
    return config
