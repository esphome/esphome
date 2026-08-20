from esphome import automation
import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_INITIAL_VALUE

from .. import CONF_MCP4461_ID, Mcp4461Component, mcp4461_ns

DEPENDENCIES = ["mcp4461"]

Mcp4461Wiper = mcp4461_ns.class_(
    "Mcp4461Wiper", output.FloatOutput, cg.Parented.template(Mcp4461Component)
)

Mcp4461WiperIdx = mcp4461_ns.enum("Mcp4461WiperIdx", is_class=True)
CHANNEL_OPTIONS = {
    "A": Mcp4461WiperIdx.MCP4461_WIPER_0,
    "B": Mcp4461WiperIdx.MCP4461_WIPER_1,
    "C": Mcp4461WiperIdx.MCP4461_WIPER_2,
    "D": Mcp4461WiperIdx.MCP4461_WIPER_3,
    "E": Mcp4461WiperIdx.MCP4461_WIPER_4,
    "F": Mcp4461WiperIdx.MCP4461_WIPER_5,
    "G": Mcp4461WiperIdx.MCP4461_WIPER_6,
    "H": Mcp4461WiperIdx.MCP4461_WIPER_7,
}

CONF_TERMINAL_A = "terminal_a"
CONF_TERMINAL_B = "terminal_b"
CONF_TERMINAL_W = "terminal_w"
CONF_NONVOLATILE = "nonvolatile"
CONF_NONVOLATILE_WRITE_DELAY = "nonvolatile_write_delay"

# Volatile wiper channels that have a nonvolatile shadow register on the chip
VOLATILE_CHANNELS = ("A", "B", "C", "D")


def _validate_nonvolatile(config) -> None:
    channel = str(config[CONF_CHANNEL])

    # Channels E-H address the nonvolatile registers directly — the mirroring options only
    # make sense for the volatile channels A-D.
    if channel not in VOLATILE_CHANNELS:
        # Only reject what the user EXPLICITLY asked for and cannot have: enabling the
        # mirroring or tuning its delay on E-H. An explicit `nonvolatile: false` is a
        # harmless no-op and stays valid; bare configs (no key at all) must keep working.
        # NOTE: FINAL_VALIDATE_SCHEMA intentionally mutates `config` in-place (uses setdefault) to apply defaults for callers.
        if config.get(CONF_NONVOLATILE) or CONF_NONVOLATILE_WRITE_DELAY in config:
            raise cv.Invalid(
                f"enabling '{CONF_NONVOLATILE}' or setting '{CONF_NONVOLATILE_WRITE_DELAY}' is only valid for the "
                f"volatile channels A-D; channels E-H are the nonvolatile registers themselves"
            )
        return

    config.setdefault(CONF_NONVOLATILE, True)
    if config[CONF_NONVOLATILE]:
        config.setdefault(
            CONF_NONVOLATILE_WRITE_DELAY,
            cv.positive_time_period_milliseconds("1s"),
        )
    elif CONF_NONVOLATILE_WRITE_DELAY in config:
        # Same consistency as the E-H rejection above: never silently ignore user input.
        raise cv.Invalid(
            f"'{CONF_NONVOLATILE_WRITE_DELAY}' requires '{CONF_NONVOLATILE}: true'"
        )


CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(Mcp4461Wiper),
        cv.GenerateID(CONF_MCP4461_ID): cv.use_id(Mcp4461Component),
        cv.Required(CONF_CHANNEL): cv.enum(CHANNEL_OPTIONS, upper=True),
        cv.Optional(CONF_TERMINAL_A, default=True): cv.boolean,
        cv.Optional(CONF_TERMINAL_B, default=True): cv.boolean,
        cv.Optional(CONF_TERMINAL_W, default=True): cv.boolean,
        cv.Optional(CONF_INITIAL_VALUE): cv.float_range(min=0.0, max=1.0),
        # No schema defaults here: a default would materialize the keys on EVERY channel,
        # making existing bare E-H configs fail final validation. The effective defaults
        # (nonvolatile: true, delay 1s) are applied for the volatile channels A-D inside
        # _validate_nonvolatile instead. Default-on rationale: the chip restores the
        # nonvolatile wiper levels at power-on, so persisting every settled level change is
        # the least surprising behavior — the pot simply comes back where it was. The write
        # is deferred by nonvolatile_write_delay to debounce transitions and protect the
        # EEPROM's endurance.
        cv.Optional(CONF_NONVOLATILE): cv.boolean,
        cv.Optional(CONF_NONVOLATILE_WRITE_DELAY): cv.positive_time_period_milliseconds,
    }
)

FINAL_VALIDATE_SCHEMA = _validate_nonvolatile


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MCP4461_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        parent,
        config[CONF_CHANNEL],
    )
    if not config[CONF_TERMINAL_A]:
        cg.add(parent.initialize_terminal_disabled(config[CONF_CHANNEL], ord("a")))
    if not config[CONF_TERMINAL_B]:
        cg.add(parent.initialize_terminal_disabled(config[CONF_CHANNEL], ord("b")))
    if not config[CONF_TERMINAL_W]:
        cg.add(parent.initialize_terminal_disabled(config[CONF_CHANNEL], ord("w")))
    if CONF_INITIAL_VALUE in config:
        cg.add(
            parent.set_initial_value(config[CONF_CHANNEL], config[CONF_INITIAL_VALUE])
        )
    if str(config[CONF_CHANNEL]) in VOLATILE_CHANNELS and config[CONF_NONVOLATILE]:
        cg.add(
            parent.set_nonvolatile(
                config[CONF_CHANNEL],
                config[CONF_NONVOLATILE_WRITE_DELAY],
            )
        )
    await output.register_output(var, config)
    await cg.register_parented(var, config[CONF_MCP4461_ID])


# ---- Actions ----
WiperIncreaseAction = mcp4461_ns.class_("WiperIncreaseAction", automation.Action)
WiperDecreaseAction = mcp4461_ns.class_("WiperDecreaseAction", automation.Action)
WiperStoreNonvolatileAction = mcp4461_ns.class_(
    "WiperStoreNonvolatileAction", automation.Action
)
WiperSetTerminalAction = mcp4461_ns.class_("WiperSetTerminalAction", automation.Action)

WIPER_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.Required(CONF_ID): cv.use_id(Mcp4461Wiper)}
)

CONF_TERMINAL = "terminal"
CONF_ENABLE = "enable"

TERMINAL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Mcp4461Wiper),
        cv.Required(CONF_TERMINAL): cv.one_of("a", "b", "w", "h", lower=True),
        cv.Required(CONF_ENABLE): cv.boolean,
    }
)


@automation.register_action(
    "mcp4461.wiper.increase", WiperIncreaseAction, WIPER_ACTION_SCHEMA, synchronous=True
)
@automation.register_action(
    "mcp4461.wiper.decrease", WiperDecreaseAction, WIPER_ACTION_SCHEMA, synchronous=True
)
async def mcp4461_wiper_step_to_code(config, action_id, template_arg, args):
    wiper = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, wiper)


@automation.register_action(
    "mcp4461.wiper.store_nonvolatile",
    WiperStoreNonvolatileAction,
    WIPER_ACTION_SCHEMA,
    synchronous=True,
)
async def mcp4461_wiper_store_to_code(config, action_id, template_arg, args):
    wiper = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, wiper)


@automation.register_action(
    "mcp4461.wiper.set_terminal",
    WiperSetTerminalAction,
    TERMINAL_ACTION_SCHEMA,
    synchronous=True,
)
async def mcp4461_wiper_terminal_to_code(config, action_id, template_arg, args):
    wiper = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(
        action_id, template_arg, wiper, ord(config[CONF_TERMINAL]), config[CONF_ENABLE]
    )
