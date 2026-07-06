from collections.abc import Callable

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_TIMEOUT, CONF_TIMEOUT
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]

provisioning_ns = cg.esphome_ns.namespace("provisioning")
ProvisioningManager = provisioning_ns.class_("ProvisioningManager", cg.Component)

# Detectors that report whether the full config provides a provisioning-capable
# source (a transport that boots unprovisioned and is set up by the controller on
# first connection). Provisioning-capable components register a detector at import
# time so `provisioning:` can validate at least one is present, without this
# component needing to know about any specific transport.
_PROVISIONING_SOURCE_DETECTORS: list[Callable[[ConfigType], bool]] = []


def register_provisioning_source(detector: Callable[[ConfigType], bool]) -> None:
    """Register a detector that reports whether the config provides a provisioning source.

    The detector receives the full validated config and returns True when its
    component is configured in a provisioning-capable way.
    """
    _PROVISIONING_SOURCE_DETECTORS.append(detector)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ProvisioningManager),
        cv.Required(CONF_TIMEOUT): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ON_TIMEOUT): automation.validate_automation(single=True),
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_has_source(config: ConfigType) -> ConfigType:
    """Require at least one provisioning-capable component when provisioning is used."""
    full_config = fv.full_config.get()
    if any(detector(full_config) for detector in _PROVISIONING_SOURCE_DETECTORS):
        return config
    raise cv.Invalid(
        "'provisioning' requires at least one provisioning-capable component. Enable "
        "'api:' with 'encryption:' and no 'key:' so the device boots unprovisioned "
        "and is configured on first connection."
    )


FINAL_VALIDATE_SCHEMA = _validate_has_source


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_PROVISIONING")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    if on_timeout := config.get(CONF_ON_TIMEOUT):
        await automation.build_automation(var.get_timeout_trigger(), [], on_timeout)
