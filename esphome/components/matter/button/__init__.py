"""`button: platform: matter` — buttons that trigger local Matter management
actions on the device itself (open a fresh commissioning window, factory
reset, etc.). Companion to the matter component in the parent directory.

Unlike ESPHome buttons defined elsewhere in the yaml, these DO NOT get
wrapped as Matter GenericSwitch endpoints. The scanner in
MatterComponent::scan_and_register_buttons_ dynamic_casts against
MatterActionButton and skips them so the fabric never sees a "click to
open my pairing window" endpoint (which would be circular).
"""

import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ACTION, CONF_TIMEOUT
from esphome.core import TimePeriod

from .. import CONF_MATTER, MatterComponent, matter_ns

CODEOWNERS = ["@gtjadsonsantos"]
DEPENDENCIES = ["matter"]

MatterActionButton = matter_ns.class_("MatterActionButton", button.Button, cg.Component)

# Yaml-facing action name → C++ enumerator name (the right column is
# resolved to the actual codegen expression only in to_code, since
# cv.enum wraps values as MockObj instances whose per-access identity
# breaks equality checks in a config-level validator).
_ACTION_ENUM = matter_ns.enum("MatterActionButton::Action", is_class=True)
_ACTION_CPP_NAMES = {
    "open_commissioning_window": "ACTION_OPEN_COMMISSIONING_WINDOW",
    "factory_reset": "ACTION_FACTORY_RESET",
}
ACTIONS = list(_ACTION_CPP_NAMES.keys())


def _validate_timeout_scope(config):
    # `timeout` is only meaningful for open_commissioning_window (spec §11.19.9.1).
    # Reject explicit timeouts on other actions so yaml doesn't quietly carry
    # dead config — the C++ side ignores it, but silent ignore is worse.
    if config[CONF_ACTION] != "open_commissioning_window" and CONF_TIMEOUT in config:
        raise cv.Invalid(
            f"'{CONF_TIMEOUT}' is only valid for action: open_commissioning_window"
        )
    return config


CONFIG_SCHEMA = cv.All(
    button.button_schema(MatterActionButton)
    .extend(
        {
            # Reuse the parent matter component's id — a device only has one
            # MatterComponent instance so cv.use_id() with GenerateID resolves
            # cleanly without the user having to name it in yaml.
            cv.GenerateID(CONF_MATTER): cv.use_id(MatterComponent),
            cv.Required(CONF_ACTION): cv.one_of(*ACTIONS, lower=True),
            # OpenCommissioningWindow spec range: 180..900s (§11.19.9.1).
            # C++ side re-clamps defensively; validating here gives a nicer
            # early error than a silent clamp at press-time. Kept Optional
            # (no default) so factory_reset users don't get spurious dead
            # values in the resolved config.
            cv.Optional(CONF_TIMEOUT): cv.All(
                cv.positive_time_period_seconds,
                cv.Range(min=TimePeriod(seconds=180), max=TimePeriod(seconds=900)),
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _validate_timeout_scope,
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_MATTER])
    cg.add(var.set_matter_component(parent))
    action_cpp = getattr(_ACTION_ENUM, _ACTION_CPP_NAMES[config[CONF_ACTION]])
    cg.add(var.set_action(action_cpp))
    # C++ default (300s in the header) covers the omitted case for
    # factory_reset — that branch never reads timeout_seconds_ anyway.
    if CONF_TIMEOUT in config:
        cg.add(var.set_timeout_seconds(int(config[CONF_TIMEOUT].total_seconds)))
