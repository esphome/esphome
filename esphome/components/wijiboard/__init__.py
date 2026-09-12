from esphome import automation
import esphome.codegen as cg
from esphome.components import stepper
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_X, CONF_Y
from esphome.core import ID
from esphome.types import ConfigType

CODEOWNERS = ["@NotWoods"]
DEPENDENCIES = ["stepper"]
MULTI_CONF = True

CONF_WIJIBOARD_ID = "wijiboard_id"

CONF_BASE_SEPARATION = "base_separation"
CONF_FOREARM_LENGTH = "forearm_length"
CONF_HOLD_TIME = "hold_time"
CONF_HOME_ON_BOOT = "home_on_boot"
CONF_HOMING_POSITIONS = "homing_positions"
CONF_LETTER = "letter"
CONF_LETTER_PAUSE = "letter_pause"
CONF_LETTERS = "letters"
CONF_MAX_WORD_LENGTH = "max_word_length"
CONF_ON_HOME = "on_home"
CONF_ON_LETTER = "on_letter"
CONF_ON_WORD_END = "on_word_end"
CONF_ON_WORD_START = "on_word_start"
CONF_REST_POSITION = "rest_position"
CONF_RETURN_HOME_BETWEEN_LETTERS = "return_home_between_letters"
CONF_SPACE_PAUSE = "space_pause"
CONF_STEPPER_1 = "stepper_1"
CONF_STEPPER_2 = "stepper_2"
CONF_STEPS_PER_ROTATION = "steps_per_rotation"
CONF_THETA1 = "theta1"
CONF_THETA2 = "theta2"
CONF_UPPER_ARM_LENGTH = "upper_arm_length"
CONF_USE_DEFAULT_LETTERS = "use_default_letters"
CONF_WORD = "word"

wijiboard_ns = cg.esphome_ns.namespace("wijiboard")
WijiBoard = wijiboard_ns.class_("WijiBoard", cg.Component)
WijiBoardLetter = wijiboard_ns.struct("WijiBoardLetter")

WriteWordAction = wijiboard_ns.class_("WriteWordAction", automation.Action)
WriteLetterAction = wijiboard_ns.class_("WriteLetterAction", automation.Action)
GotoXYAction = wijiboard_ns.class_("GotoXYAction", automation.Action)
GotoAnglesAction = wijiboard_ns.class_("GotoAnglesAction", automation.Action)
HomeAction = wijiboard_ns.class_("HomeAction", automation.Action)
StopAction = wijiboard_ns.class_("StopAction", automation.Action)
IsBusyCondition = wijiboard_ns.class_("IsBusyCondition", automation.Condition)

# Board coordinates in mm, measured from the middle of the board, as used by the original
# WijiBoard firmware. Override individual entries under `letters:` to match your own artwork.
DEFAULT_LETTERS: dict[str, tuple[float, float]] = {
    "Q": (-66.5, 91.6),
    "W": (70.8, 92.0),
    "E": (-41.4, 125.5),
    "R": (-44.0, 96.0),
    "T": (1.3, 97.8),
    "Y": (117.8, 91.7),
    "U": (22.5, 98.3),
    "I": (53.5, 124.5),
    "O": (-112.0, 82.0),
    "P": (-90.0, 89.0),
    "A": (-143.0, 99.0),
    "S": (-19.8, 98.0),
    "D": (-68.5, 121.7),
    "F": (-19.5, 127.0),
    "G": (4.0, 128.4),
    "H": (31.3, 127.3),
    "J": (72.5, 121.0),
    "K": (93.0, 116.8),
    "L": (114.3, 110.2),
    "Z": (130.0, 75.0),
    "X": (95.9, 87.3),
    "C": (-94.0, 116.0),
    "V": (44.5, 97.0),
    "B": (-119.0, 109.0),
    "N": (-135.4, 75.1),
    "M": (132.0, 100.5),
    "+": (-110.0, 1.0),
    "-": (110.0, 1.0),
    "*": (110.0, -24.0),
    "0": (-130.4, 44.2),
    "1": (-101.1, 53.3),
    "2": (-71.5, 60.4),
    "3": (-41.7, 65.2),
    "4": (-13.0, 66.5),
    "5": (16.3, 68.0),
    "6": (45.8, 64.0),
    "7": (75.7, 61.1),
    "8": (103.6, 53.8),
    "9": (132.1, 45.0),
}

# The comma sits outside the reach of the linkage, so the original firmware drove it with a
# hand-measured pair of arm angles instead of solving for it.
DEFAULT_LETTER_ANGLES: dict[str, tuple[float, float]] = {
    ",": (130.0, 268.0),
}


def _validate_letter_key(value: str) -> str:
    value = cv.string_strict(value)
    if len(value) != 1:
        raise cv.Invalid(
            f"A character map key must be exactly one character, got {value!r}"
        )
    if value == " ":
        raise cv.Invalid(
            f"A space is always a pause, configure its length with '{CONF_SPACE_PAUSE}'"
        )
    return value.upper()


LETTER_SCHEMA = cv.Any(
    cv.Schema(
        {
            cv.Required(CONF_X): cv.float_,
            cv.Required(CONF_Y): cv.float_,
        }
    ),
    cv.Schema(
        {
            cv.Required(CONF_THETA1): cv.float_,
            cv.Required(CONF_THETA2): cv.float_,
        }
    ),
)


def _validate_letters(config: ConfigType) -> ConfigType:
    letters: dict[str, ConfigType] = {}
    if config[CONF_USE_DEFAULT_LETTERS]:
        letters = {
            key: {CONF_X: x, CONF_Y: y} for key, (x, y) in DEFAULT_LETTERS.items()
        } | {
            key: {CONF_THETA1: a, CONF_THETA2: b}
            for key, (a, b) in DEFAULT_LETTER_ANGLES.items()
        }
    letters.update(config.get(CONF_LETTERS, {}))
    if not letters:
        raise cv.Invalid(
            f"No characters configured; set '{CONF_LETTERS}' or leave "
            f"'{CONF_USE_DEFAULT_LETTERS}' enabled",
            path=[CONF_LETTERS],
        )
    if len(letters) > 255:
        raise cv.Invalid("At most 255 characters can be mapped", path=[CONF_LETTERS])
    config[CONF_LETTERS] = letters
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WijiBoard),
            cv.Required(CONF_STEPPER_1): cv.use_id(stepper.Stepper),
            cv.Required(CONF_STEPPER_2): cv.use_id(stepper.Stepper),
            cv.Optional(CONF_BASE_SEPARATION, default=25.8): cv.positive_float,
            cv.Optional(
                CONF_UPPER_ARM_LENGTH, default=85.0
            ): cv.positive_not_null_float,
            cv.Optional(CONF_FOREARM_LENGTH, default=110.0): cv.positive_not_null_float,
            cv.Optional(CONF_STEPS_PER_ROTATION, default=2048): cv.int_range(
                min=1, max=65535
            ),
            cv.Optional(CONF_REST_POSITION, default=[-1024, 0]): cv.All(
                [cv.int_], cv.Length(min=2, max=2)
            ),
            cv.Optional(
                CONF_HOMING_POSITIONS, default=[1024, 2048, -1050, -1300, 550, -530]
            ): cv.All([cv.int_], cv.Length(min=6, max=6)),
            cv.Optional(
                CONF_HOLD_TIME, default="500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_LETTER_PAUSE, default="200ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_SPACE_PAUSE, default="1s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_RETURN_HOME_BETWEEN_LETTERS, default=True): cv.boolean,
            cv.Optional(CONF_HOME_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_MAX_WORD_LENGTH, default=64): cv.int_range(min=1, max=255),
            cv.Optional(CONF_USE_DEFAULT_LETTERS, default=True): cv.boolean,
            cv.Optional(CONF_LETTERS): cv.Schema({_validate_letter_key: LETTER_SCHEMA}),
            cv.Optional(CONF_ON_WORD_START): automation.validate_automation({}),
            cv.Optional(CONF_ON_LETTER): automation.validate_automation({}),
            cv.Optional(CONF_ON_WORD_END): automation.validate_automation({}),
            cv.Optional(CONF_ON_HOME): automation.validate_automation({}),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_letters,
)

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_WORD_START, "add_on_word_start_callback", [(cg.std_string, "x")]
    ),
    automation.CallbackAutomation(
        CONF_ON_LETTER, "add_on_letter_callback", [(cg.uint8, "x")]
    ),
    automation.CallbackAutomation(CONF_ON_WORD_END, "add_on_word_end_callback"),
    automation.CallbackAutomation(CONF_ON_HOME, "add_on_home_callback"),
)


def _letter_initializer(key: str, entry: ConfigType) -> cg.RawExpression:
    escaped = key.replace("\\", "\\\\").replace("'", "\\'")
    if CONF_THETA1 in entry:
        return cg.RawExpression(
            f"{{'{escaped}', true, {entry[CONF_THETA1]}f, {entry[CONF_THETA2]}f}}"
        )
    return cg.RawExpression(
        f"{{'{escaped}', false, {entry[CONF_X]}f, {entry[CONF_Y]}f}}"
    )


async def to_code(config: ConfigType) -> None:
    stepper_1 = await cg.get_variable(config[CONF_STEPPER_1])
    stepper_2 = await cg.get_variable(config[CONF_STEPPER_2])
    var = cg.new_Pvariable(config[CONF_ID], stepper_1, stepper_2)
    await cg.register_component(var, config)

    letters = config[CONF_LETTERS]
    letters_id = ID(
        f"{config[CONF_ID].id}_letters", is_declaration=True, type=WijiBoardLetter
    )
    letters_array = cg.static_const_array(
        letters_id,
        cg.ArrayInitializer(
            *(_letter_initializer(key, entry) for key, entry in letters.items()),
            multiline=True,
        ),
    )
    cg.add(var.set_letters(letters_array, len(letters)))

    cg.add(
        var.set_geometry(
            config[CONF_BASE_SEPARATION],
            config[CONF_UPPER_ARM_LENGTH],
            config[CONF_FOREARM_LENGTH],
        )
    )
    cg.add(var.set_steps_per_rotation(config[CONF_STEPS_PER_ROTATION]))
    cg.add(var.set_rest_position(*config[CONF_REST_POSITION]))
    cg.add(
        var.set_homing_positions(cg.ArrayInitializer(*config[CONF_HOMING_POSITIONS]))
    )
    cg.add(var.set_hold_time(config[CONF_HOLD_TIME]))
    cg.add(var.set_letter_pause(config[CONF_LETTER_PAUSE]))
    cg.add(var.set_space_pause(config[CONF_SPACE_PAUSE]))
    cg.add(
        var.set_return_home_between_letters(config[CONF_RETURN_HOME_BETWEEN_LETTERS])
    )
    cg.add(var.set_home_on_boot(config[CONF_HOME_ON_BOOT]))

    cg.add_define("WIJIBOARD_MAX_WORD_LENGTH", config[CONF_MAX_WORD_LENGTH])

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


WIJIBOARD_ACTION_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(WijiBoard)})


@automation.register_action(
    "wijiboard.write_word",
    WriteWordAction,
    cv.maybe_simple_value(
        WIJIBOARD_ACTION_SCHEMA.extend(
            {cv.Required(CONF_WORD): cv.templatable(cv.string)}
        ),
        key=CONF_WORD,
    ),
    synchronous=True,
)
async def wijiboard_write_word_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_WORD], args, cg.std_string)
    cg.add(var.set_word(template_))
    return var


@automation.register_action(
    "wijiboard.write_letter",
    WriteLetterAction,
    cv.maybe_simple_value(
        WIJIBOARD_ACTION_SCHEMA.extend(
            {cv.Required(CONF_LETTER): cv.templatable(cv.string)}
        ),
        key=CONF_LETTER,
    ),
    synchronous=True,
)
async def wijiboard_write_letter_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_LETTER], args, cg.std_string)
    cg.add(var.set_letter(template_))
    return var


@automation.register_action(
    "wijiboard.goto_xy",
    GotoXYAction,
    WIJIBOARD_ACTION_SCHEMA.extend(
        {
            cv.Required(CONF_X): cv.templatable(cv.float_),
            cv.Required(CONF_Y): cv.templatable(cv.float_),
        }
    ),
    synchronous=True,
)
async def wijiboard_goto_xy_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_x(await cg.templatable(config[CONF_X], args, cg.float_)))
    cg.add(var.set_y(await cg.templatable(config[CONF_Y], args, cg.float_)))
    return var


@automation.register_action(
    "wijiboard.goto_angles",
    GotoAnglesAction,
    WIJIBOARD_ACTION_SCHEMA.extend(
        {
            cv.Required(CONF_THETA1): cv.templatable(cv.float_),
            cv.Required(CONF_THETA2): cv.templatable(cv.float_),
        }
    ),
    synchronous=True,
)
async def wijiboard_goto_angles_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_theta1(await cg.templatable(config[CONF_THETA1], args, cg.float_)))
    cg.add(var.set_theta2(await cg.templatable(config[CONF_THETA2], args, cg.float_)))
    return var


@automation.register_action(
    "wijiboard.home",
    HomeAction,
    cv.maybe_simple_value(WIJIBOARD_ACTION_SCHEMA, key=CONF_ID),
    synchronous=True,
)
async def wijiboard_home_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "wijiboard.stop",
    StopAction,
    cv.maybe_simple_value(WIJIBOARD_ACTION_SCHEMA, key=CONF_ID),
    synchronous=True,
)
async def wijiboard_stop_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_condition(
    "wijiboard.is_busy",
    IsBusyCondition,
    cv.maybe_simple_value(WIJIBOARD_ACTION_SCHEMA, key=CONF_ID),
)
async def wijiboard_is_busy_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
