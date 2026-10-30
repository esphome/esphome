from collections.abc import Callable
from dataclasses import dataclass

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import power_supply
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_ID,
    CONF_INVERTED,
    CONF_LEVEL,
    CONF_MAX_POWER,
    CONF_MIN_POWER,
    CONF_POWER_SUPPLY,
)
from esphome.core import CORE, ID
from esphome.core.entity_helpers import ApplyEntity, SubEntities
from esphome.cpp_generator import MockObj, SafeExpType
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True
DOMAIN = "output"

CONF_ZERO_MEANS_ZERO = "zero_means_zero"


@dataclass
class OutputData:
    power_scaling: bool = False


def _get_data() -> OutputData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = OutputData()
    return CORE.data[DOMAIN]


BINARY_OUTPUT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_POWER_SUPPLY): cv.use_id(power_supply.PowerSupply),
        cv.Optional(CONF_INVERTED): cv.boolean,
    }
)

FLOAT_OUTPUT_SCHEMA = BINARY_OUTPUT_SCHEMA.extend(
    {
        cv.Optional(CONF_MAX_POWER): cv.percentage,
        cv.Optional(CONF_MIN_POWER): cv.percentage,
        cv.Optional(CONF_ZERO_MEANS_ZERO, default=False): cv.boolean,
    }
)

output_ns = cg.esphome_ns.namespace("output")
BinaryOutput = output_ns.class_("BinaryOutput")
BinaryOutputPtr = BinaryOutput.operator("ptr")
FloatOutput = output_ns.class_("FloatOutput", BinaryOutput)
FloatOutputPtr = FloatOutput.operator("ptr")
ApplyBinaryOutput = output_ns.class_("ApplyBinaryOutput", BinaryOutput)
ApplyFloatOutput = output_ns.class_("ApplyFloatOutput", FloatOutput)


async def setup_output_platform_(obj, config):
    cg.add_optional(obj.set_inverted, config.get(CONF_INVERTED))
    await cg.add_variable(obj.set_power_supply, config.get(CONF_POWER_SUPPLY))
    # The C++ initializers are max_power 1.0 and min_power 0.0; skip the setter when
    # the config matches them. The define stays whenever the key is present because
    # platforms such as ac_dimmer read the scaling fields directly.
    if (max_power := config.get(CONF_MAX_POWER)) is not None:
        cg.add_define("USE_OUTPUT_FLOAT_POWER_SCALING")
        if max_power != 1.0:
            cg.add(obj.set_max_power(max_power))
    if (min_power := config.get(CONF_MIN_POWER)) is not None:
        cg.add_define("USE_OUTPUT_FLOAT_POWER_SCALING")
        if min_power != 0.0:
            cg.add(obj.set_min_power(min_power))
    # Only emit when zero_means_zero is actually enabled. The schema defaults to False
    # so this key is always present; emitting unconditionally would force
    # USE_OUTPUT_FLOAT_POWER_SCALING on for every output, defeating the gate.
    if config.get(CONF_ZERO_MEANS_ZERO):
        cg.add_define("USE_OUTPUT_FLOAT_POWER_SCALING")
        cg.add(obj.set_zero_means_zero(config[CONF_ZERO_MEANS_ZERO]))


async def register_output(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    await setup_output_platform_(var, config)


async def new_output(config: ConfigType, *args: SafeExpType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_output(var, config)
    return var


BINARY_APPLY_ENTITY = ApplyEntity(
    ApplyBinaryOutput,
    ((BinaryOutputPtr, "entity"), (cg.uint16, "index"), (cg.bool_, "state")),
)
FLOAT_APPLY_ENTITY = ApplyEntity(
    ApplyFloatOutput,
    ((FloatOutputPtr, "entity"), (cg.uint16, "index"), (cg.float_, "state")),
)


def sub_binary_outputs(
    config: ConfigType, *, parent: MockObj | ID | None = None
) -> SubEntities:
    """Return a SubEntities bound to new_output; ``apply`` can use ``entity``, ``index`` and ``state``.

    ``index`` is the constructor argument (default 0), so a hub's channels share one class.
    """
    return SubEntities(new_output, config, parent, BINARY_APPLY_ENTITY)


def sub_float_outputs(
    config: ConfigType, *, parent: MockObj | ID | None = None
) -> SubEntities:
    """Like sub_binary_outputs; ``apply`` gets the scaled level as ``state``."""
    return SubEntities(new_output, config, parent, FLOAT_APPLY_ENTITY)


BINARY_OUTPUT_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(BinaryOutput),
    }
)


def final_validate_unique_channel(
    *, platform: str, hub_key: str, channel_key: str = CONF_CHANNEL
) -> Callable[[ConfigType], ConfigType]:
    """Reject a second output of platform on the same channel of one hub.

    Two outputs on one channel would overwrite each other's value.
    """
    return fv.unique_platform_key(
        domain=DOMAIN,
        platform=platform,
        keys=(hub_key, channel_key),
        message="Channel '{1}' of '{0}' is already used by another output",
    )


def _enable_power_scaling(config: ConfigType) -> ConfigType:
    # set_min_power/set_max_power only exist with the define; to_code emits it from this fact.
    _get_data().power_scaling = True
    return config


for _name, _call in (
    ("output.turn_on", "turn_on()"),
    ("output.turn_off", "turn_off()"),
):
    automation.register_apply_action(
        _name, BINARY_OUTPUT_ACTION_SCHEMA, automation.ApplyCall(_call)
    )

automation.register_apply_action(
    "output.set_level",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(FloatOutput),
            cv.Required(CONF_LEVEL): cv.templatable(cv.percentage),
        }
    ),
    automation.ApplyField(CONF_LEVEL, "set_level", cg.float_),
)

for _name, _key, _target in (
    ("output.set_min_power", CONF_MIN_POWER, "set_min_power"),
    ("output.set_max_power", CONF_MAX_POWER, "set_max_power"),
):
    automation.register_apply_action(
        _name,
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(FloatOutput),
                cv.Required(_key): cv.templatable(cv.percentage),
            }
        ).add_extra(_enable_power_scaling),
        automation.ApplyField(_key, _target, cg.float_),
    )


async def to_code(config):
    cg.add_define("USE_OUTPUT")
    if _get_data().power_scaling:
        cg.add_define("USE_OUTPUT_FLOAT_POWER_SCALING")
    cg.add_global(output_ns.using)
