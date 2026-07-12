from dataclasses import dataclass, field
import logging

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_TIMEOUT, CONF_TIMEOUT
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
DOMAIN = "provisioning"

_LOGGER = logging.getLogger(__name__)

provisioning_ns = cg.esphome_ns.namespace("provisioning")
ProvisioningManager = provisioning_ns.class_("ProvisioningManager", cg.Component)


@dataclass
class ProvisioningData:
    # Names of the components that registered as a provisioning source this run.
    sources: set[str] = field(default_factory=set)
    # Names of source components that have their credentials set in the config.
    hardcoded_credentials: set[str] = field(default_factory=set)


def _get_data() -> ProvisioningData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = ProvisioningData()
    return CORE.data[DOMAIN]


def register_source(name: str) -> None:
    """Record that ``name`` is a provisioning source for this configuration.

    A provisioning-capable component (a transport that boots unprovisioned and is
    set up by the controller on first connection, or a network interface that
    provisions once connected) calls this while its own config is being processed,
    typically from a schema validator. `provisioning:` then confirms at least one
    source is present without inspecting the full config or knowing about any
    specific component. State lives in CORE.data, which is cleared between runs.
    """
    _get_data().sources.add(name)


def report_hardcoded_credentials(name: str) -> None:
    """Record that source component ``name`` has its credentials set in the config.

    A source component calls this from its own validator when it finds baked-in
    credentials (a WiFi SSID/password, an API encryption key, ...). `provisioning:`
    warns about these, since a device that ships with credentials does not need a
    provisioning window. The warning is emitted here, by `provisioning:`, so the
    source components stay unaware of it.
    """
    _get_data().hardcoded_credentials.add(name)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ProvisioningManager),
        cv.Required(CONF_TIMEOUT): cv.All(
            cv.positive_not_null_time_period, cv.positive_time_period_milliseconds
        ),
        cv.Optional(CONF_ON_TIMEOUT): automation.validate_automation(single=True),
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config: ConfigType) -> ConfigType:
    """Validate the provisioning setup once every component has been processed.

    Sources register during their own config validation, so by final validation
    both the source set and the hardcoded-credentials set are complete.
    """
    data = _get_data()
    if not data.sources:
        raise cv.Invalid(
            "'provisioning' requires at least one provisioning-capable component: "
            "configure a network interface such as 'wifi:' or 'ethernet:', or enable "
            "'api:' with 'encryption:' and no 'key:' so the device boots "
            "unprovisioned and is configured on first connection."
        )
    if data.hardcoded_credentials:
        _LOGGER.warning(
            "'provisioning' is configured, but credentials are set in the "
            "configuration for: %s. A device that uses a provisioning window should "
            "ship without credentials so they are set on first connection; "
            "hardcoding them makes the window pointless.",
            ", ".join(sorted(data.hardcoded_credentials)),
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_PROVISIONING")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    if on_timeout := config.get(CONF_ON_TIMEOUT):
        await automation.build_automation(var.get_timeout_trigger(), [], on_timeout)
