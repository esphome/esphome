import logging

import esphome.codegen as cg
from esphome.components import switch
from esphome.components.const import CONF_ENABLED
from esphome.components.switch import DOMAIN as SWITCH_DOMAIN
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_SENDSPIN_ID,
    CONF_STATIC_PAIRING_CODE,
    CONF_UNPAIRED_ACCESS,
    DOMAIN,
    SendspinHub,
    _has_pairing_method,
    request_switch,
    sendspin_ns,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["sendspin"]

SendspinUnpairedAccessSwitch = sendspin_ns.class_(
    "SendspinUnpairedAccessSwitch", switch.Switch, cg.Component
)


def _switch_schema(class_: cg.MockObjClass) -> cv.Schema:
    return switch.switch_schema(
        class_,
        block_inverted=True,
        default_restore_mode="RESTORE_DEFAULT_ON",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ).extend({cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub)})


def _request_switch(config: ConfigType) -> ConfigType:
    """Tell the hub to wait for this switch before the client's first start."""
    request_switch(config[CONF_TYPE])
    return config


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            CONF_ENABLED: _switch_schema(switch.ApplySwitch),
            CONF_UNPAIRED_ACCESS: _switch_schema(SendspinUnpairedAccessSwitch).extend(
                cv.COMPONENT_SCHEMA
            ),
        },
        key=CONF_TYPE,
    ),
    cv.only_on_esp32,
    _request_switch,
)


# Two switches of one type would each drive the same hub setting.
_validate_unique_type = fv.unique_platform_key(
    domain=SWITCH_DOMAIN,
    platform=DOMAIN,
    keys=(CONF_TYPE,),
    message="Only one sendspin '{type}' switch is allowed",
)


def _final_validate(config: ConfigType) -> ConfigType:
    _validate_unique_type(config)
    if config[CONF_TYPE] != CONF_UNPAIRED_ACCESS:
        return config
    hub_config = fv.full_config.get().get(DOMAIN, {})
    if CONF_UNPAIRED_ACCESS in hub_config:
        raise cv.Invalid(
            f"'{DOMAIN}: {CONF_UNPAIRED_ACCESS}' cannot be set together with an "
            f"{CONF_UNPAIRED_ACCESS} switch; set the switch's restore_mode instead"
        )
    if not _has_pairing_method(hub_config):
        _LOGGER.warning(
            "The %s switch has no pairing method (%s or a dynamic pairing code), so "
            "while it is off no new server can pair with this device",
            CONF_UNPAIRED_ACCESS,
            CONF_STATIC_PAIRING_CODE,
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    if config[CONF_TYPE] == CONF_ENABLED:
        # The hub restores this switch in its setup() and logs it.
        hub = await cg.get_variable(config[CONF_SENDSPIN_ID])
        switches = switch.sub_switches(config, parent=hub)
        await switches.create(
            config, hub.set_enabled_switch, apply="set_enabled(state)", publish="after"
        )
        cg.add_define("USE_SENDSPIN_ENABLED_SWITCH")
        return
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])
    cg.add_define("USE_SENDSPIN_SWITCH", True)
