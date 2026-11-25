import esphome.codegen as cg
from esphome.components.api import wizard
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DOMAIN = "template"

CONF_WIZARD_ROLE = "wizard_role"

template_ns = cg.esphome_ns.namespace("template_")


def _wizard_role(value: str) -> str:
    """Validate a wizard role set in the configuration."""
    if (
        len(value) > wizard.WIZARD_ROLE_MAX_LENGTH
        or not wizard.WIZARD_ROLE_PATTERN.fullmatch(value)
        or not value.startswith(f"{DOMAIN}.")
    ):
        raise cv.Invalid(
            f"A wizard role of a template entity is {DOMAIN}.<name>: at most "
            f"{wizard.WIZARD_ROLE_MAX_LENGTH} characters, with a name of lowercase letters, "
            "digits and underscores"
        )
    return value


WIZARD_ROLE_SCHEMA = cv.Schema(
    {cv.Optional(CONF_WIZARD_ROLE): cv.All(cv.string_strict, _wizard_role)}
)


def register_wizard_role(config: ConfigType) -> ConfigType:
    """Give the entity the wizard role it sets, if any."""
    if (role := config.get(CONF_WIZARD_ROLE)) is not None:
        wizard.register_entity_role(config[CONF_ID], role)
    return config
