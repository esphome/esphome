from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
from esphome.components.const import CONF_LOOP
import esphome.config_validation as cv
from esphome.const import CONF_DEVICE, CONF_FILE, CONF_ID, CONF_VOLUME

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@glmnet"]

dfplayer_ns = cg.esphome_ns.namespace("dfplayer")
DFPlayer = dfplayer_ns.class_("DFPlayer", cg.Component)
DFPlayerIsPlayingCondition = dfplayer_ns.class_(
    "DFPlayerIsPlayingCondition", automation.Condition
)

MULTI_CONF = True
CONF_FOLDER = "folder"
CONF_ENABLE = "enable"
CONF_EQ_PRESET = "eq_preset"
CONF_ON_FINISHED_PLAYBACK = "on_finished_playback"

EqPreset = dfplayer_ns.enum("EqPreset")
EQ_PRESET = {
    "NORMAL": EqPreset.NORMAL,
    "POP": EqPreset.POP,
    "ROCK": EqPreset.ROCK,
    "JAZZ": EqPreset.JAZZ,
    "CLASSIC": EqPreset.CLASSIC,
    "BASS": EqPreset.BASS,
}
Device = dfplayer_ns.enum("Device")
DEVICE = {
    "USB": Device.USB,
    "TF_CARD": Device.TF_CARD,
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DFPlayer),
            cv.Optional(CONF_ON_FINISHED_PLAYBACK): automation.validate_automation({}),
        }
    ).extend(uart.UART_DEVICE_SCHEMA)
)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "dfplayer",
    baud_rate=9600,
    require_tx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_FINISHED_PLAYBACK, "add_on_finished_playback_callback"
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


DFPLAYER_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DFPlayer),
    }
)

for _name, _call in (
    ("dfplayer.play_next", "next()"),
    ("dfplayer.play_previous", "previous()"),
    ("dfplayer.volume_up", "volume_up()"),
    ("dfplayer.volume_down", "volume_down()"),
    ("dfplayer.sleep", "sleep()"),
    ("dfplayer.reset", "reset()"),
    ("dfplayer.start", "start()"),
    ("dfplayer.pause", "pause()"),
    ("dfplayer.stop", "stop()"),
    ("dfplayer.random", "random()"),
):
    automation.register_apply_action(
        _name, DFPLAYER_ACTION_SCHEMA, automation.ApplyCall(_call)
    )

automation.register_apply_action(
    "dfplayer.play_mp3",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(DFPlayer),
            cv.Required(CONF_FILE): cv.templatable(cv.int_),
        },
        key=CONF_FILE,
    ),
    automation.ApplyField(CONF_FILE, "play_mp3", cg.uint16),
)

# loop and file default to what the old action's unset templatable values evaluated to
automation.register_apply_action(
    "dfplayer.play",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(DFPlayer),
            cv.Required(CONF_FILE): cv.templatable(cv.int_),
            cv.Optional(CONF_LOOP, default=False): cv.templatable(cv.boolean),
        },
        key=CONF_FILE,
    ),
    automation.ApplyCall(
        "play_file({}, {})", ((CONF_FILE, cg.uint16), (CONF_LOOP, cg.bool_))
    ),
)

automation.register_apply_action(
    "dfplayer.play_folder",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DFPlayer),
            cv.Required(CONF_FOLDER): cv.templatable(cv.int_),
            cv.Optional(CONF_FILE, default=0): cv.templatable(cv.int_),
            cv.Optional(CONF_LOOP, default=False): cv.templatable(cv.boolean),
        }
    ),
    automation.ApplyCall(
        "play_folder({}, {}, {})",
        ((CONF_FOLDER, cg.uint16), (CONF_FILE, cg.uint16), (CONF_LOOP, cg.bool_)),
    ),
)

automation.register_apply_action(
    "dfplayer.set_device",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(DFPlayer),
            cv.Required(CONF_DEVICE): cv.enum(DEVICE, upper=True),
        },
        key=CONF_DEVICE,
    ),
    automation.ApplyField(CONF_DEVICE, "set_device", Device),
)

automation.register_apply_action(
    "dfplayer.set_volume",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(DFPlayer),
            cv.Required(CONF_VOLUME): cv.templatable(cv.int_),
        },
        key=CONF_VOLUME,
    ),
    automation.ApplyField(CONF_VOLUME, "set_volume", cg.uint8),
)

automation.register_apply_action(
    "dfplayer.set_eq",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(DFPlayer),
            cv.Required(CONF_EQ_PRESET): cv.templatable(cv.enum(EQ_PRESET, upper=True)),
        },
        key=CONF_EQ_PRESET,
    ),
    automation.ApplyField(CONF_EQ_PRESET, "set_eq", EqPreset),
)


def _default_enable(value: Any) -> Any:
    """Fill in ``enable: true`` for a bare action or a mapping that only picks the player.

    Done before ``maybe_simple_value`` so neither form is wrapped as the ``enable`` value.
    """
    if value is None or isinstance(value, dict):
        return {CONF_ENABLE: True, **(value or {})}
    return value


automation.register_apply_action(
    "dfplayer.set_current_track_repeat",
    cv.All(
        _default_enable,
        cv.maybe_simple_value(
            {
                cv.GenerateID(): cv.use_id(DFPlayer),
                cv.Optional(CONF_ENABLE, default=True): cv.templatable(cv.boolean),
            },
            key=CONF_ENABLE,
        ),
    ),
    automation.ApplyField(CONF_ENABLE, "set_current_track_repeat", cg.bool_),
)


automation.register_parented_condition(
    "dfplayer.is_playing",
    DFPlayerIsPlayingCondition,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DFPlayer),
        }
    ),
)
