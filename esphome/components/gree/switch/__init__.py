import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_LIGHT, DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv
from esphome.types import ConfigType

from ..climate import CONF_MODEL, GreeClimate

CODEOWNERS = ["@nagyrobi"]

CONF_TURBO = "turbo"
CONF_HEALTH = "health"
CONF_XFAN = "xfan"
CONF_GREE_ID = "gree_id"

SWITCH_ICONS = {
    CONF_TURBO: "mdi:car-turbocharger",
    CONF_LIGHT: "mdi:led-outline",
    CONF_HEALTH: "mdi:pine-tree",
    CONF_XFAN: "mdi:wall-sconce-flat",
}

SUPPORTED_MODELS = {
    "yan",
    "yaa",
    "yac",
    "yac1fb9",
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GREE_ID): cv.use_id(GreeClimate),
        **{
            cv.Optional(key): switch.switch_schema(
                switch.ApplySwitch,
                icon=icon,
                # The display light was on before the switches existed, so it stays on by default
                default_restore_mode="RESTORE_DEFAULT_ON"
                if key == CONF_LIGHT
                else "RESTORE_DEFAULT_OFF",
                device_class=DEVICE_CLASS_SWITCH,
                entity_category=ENTITY_CATEGORY_CONFIG,
            )
            for key, icon in SWITCH_ICONS.items()
        },
    }
)


def _validate_model(config: ConfigType) -> None:
    climate_conf = fv.get_declaring_config(config[CONF_GREE_ID])
    if climate_conf[CONF_MODEL] not in SUPPORTED_MODELS:
        raise cv.Invalid(
            "Gree switches are only supported for the "
            + ", ".join(SUPPORTED_MODELS)
            + " models"
        )


FINAL_VALIDATE_SCHEMA = _validate_model


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_GREE_ID])
    cg.add_define("USE_GREE_SWITCH")
    switches = switch.sub_switches(config, parent=hub)
    await switches(
        CONF_TURBO,
        hub.set_turbo_switch,
        apply="set_mode_bit(gree::GREE_MODE_BIT_TURBO, state)",
        publish="after",
    )
    await switches(
        CONF_LIGHT,
        hub.set_light_switch,
        apply="set_mode_bit(gree::GREE_MODE_BIT_LIGHT, state)",
        publish="after",
    )
    await switches(
        CONF_HEALTH,
        hub.set_health_switch,
        apply="set_mode_bit(gree::GREE_MODE_BIT_HEALTH, state)",
        publish="after",
    )
    await switches(
        CONF_XFAN,
        hub.set_xfan_switch,
        apply="set_mode_bit(gree::GREE_MODE_BIT_XFAN, state)",
        publish="after",
    )
