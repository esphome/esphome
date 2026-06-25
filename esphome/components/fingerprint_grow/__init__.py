from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR,
    CONF_COUNT,
    CONF_FINGER_ID,
    CONF_ID,
    CONF_NEW_PASSWORD,
    CONF_NUM_SCANS,
    CONF_ON_ENROLLMENT_DONE,
    CONF_ON_ENROLLMENT_FAILED,
    CONF_ON_ENROLLMENT_SCAN,
    CONF_ON_FINGER_SCAN_INVALID,
    CONF_ON_FINGER_SCAN_MATCHED,
    CONF_ON_FINGER_SCAN_MISPLACED,
    CONF_ON_FINGER_SCAN_START,
    CONF_ON_FINGER_SCAN_UNMATCHED,
    CONF_PASSWORD,
    CONF_SENSING_PIN,
    CONF_SPEED,
    CONF_STATE,
)

CODEOWNERS = ["@OnFreund", "@loongyh", "@alexborro"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor"]
MULTI_CONF = True

CONF_FINGERPRINT_GROW_ID = "fingerprint_grow_id"
CONF_SENSOR_POWER_PIN = "sensor_power_pin"
CONF_IDLE_PERIOD_TO_SLEEP = "idle_period_to_sleep"

fingerprint_grow_ns = cg.esphome_ns.namespace("fingerprint_grow")
FingerprintGrowComponent = fingerprint_grow_ns.class_(
    "FingerprintGrowComponent", cg.PollingComponent, uart.UARTDevice
)

EnrollmentAction = fingerprint_grow_ns.class_("EnrollmentAction", automation.Action)
CancelEnrollmentAction = fingerprint_grow_ns.class_(
    "CancelEnrollmentAction", automation.Action
)
DeleteAction = fingerprint_grow_ns.class_("DeleteAction", automation.Action)
DeleteAllAction = fingerprint_grow_ns.class_("DeleteAllAction", automation.Action)
LEDControlAction = fingerprint_grow_ns.class_("LEDControlAction", automation.Action)
AuraLEDControlAction = fingerprint_grow_ns.class_(
    "AuraLEDControlAction", automation.Action
)

AuraLEDState = fingerprint_grow_ns.enum("GrowAuraLEDState", True)
AURA_LED_STATES = {
    "BREATHING": AuraLEDState.BREATHING,
    "FLASHING": AuraLEDState.FLASHING,
    "ALWAYS_ON": AuraLEDState.ALWAYS_ON,
    "ALWAYS_OFF": AuraLEDState.ALWAYS_OFF,
    "GRADUAL_ON": AuraLEDState.GRADUAL_ON,
    "GRADUAL_OFF": AuraLEDState.GRADUAL_OFF,
}
validate_aura_led_states = cv.enum(AURA_LED_STATES, upper=True)
AuraLEDColor = fingerprint_grow_ns.enum("GrowAuraLEDColor", True)
AURA_LED_COLORS = {
    "RED": AuraLEDColor.RED,
    "BLUE": AuraLEDColor.BLUE,
    "PURPLE": AuraLEDColor.PURPLE,
    "GREEN": AuraLEDColor.GREEN,
    "YELLOW": AuraLEDColor.YELLOW,
    "CYAN": AuraLEDColor.CYAN,
    "WHITE": AuraLEDColor.WHITE,
}
validate_aura_led_colors = cv.enum(AURA_LED_COLORS, upper=True)


def validate(config):
    if CONF_SENSOR_POWER_PIN in config and CONF_SENSING_PIN not in config:
        raise cv.Invalid("You cannot use the Sensor Power Pin without a Sensing Pin")
    if CONF_IDLE_PERIOD_TO_SLEEP in config and CONF_SENSOR_POWER_PIN not in config:
        raise cv.Invalid(
            "You cannot have an Idle Period to Sleep without a Sensor Power Pin"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FingerprintGrowComponent),
            cv.Optional(CONF_SENSING_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_SENSOR_POWER_PIN): pins.gpio_output_pin_schema,
            cv.Optional(
                CONF_IDLE_PERIOD_TO_SLEEP
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PASSWORD): cv.uint32_t,
            cv.Optional(CONF_NEW_PASSWORD): cv.uint32_t,
            cv.Optional(CONF_ON_FINGER_SCAN_START): automation.validate_automation({}),
            cv.Optional(CONF_ON_FINGER_SCAN_MATCHED): automation.validate_automation(
                {}
            ),
            cv.Optional(CONF_ON_FINGER_SCAN_UNMATCHED): automation.validate_automation(
                {}
            ),
            cv.Optional(CONF_ON_FINGER_SCAN_MISPLACED): automation.validate_automation(
                {}
            ),
            cv.Optional(CONF_ON_FINGER_SCAN_INVALID): automation.validate_automation(
                {}
            ),
            cv.Optional(CONF_ON_ENROLLMENT_SCAN): automation.validate_automation({}),
            cv.Optional(CONF_ON_ENROLLMENT_DONE): automation.validate_automation({}),
            cv.Optional(CONF_ON_ENROLLMENT_FAILED): automation.validate_automation({}),
        }
    )
    .extend(cv.polling_component_schema("500ms"))
    .extend(uart.UART_DEVICE_SCHEMA),
    validate,
)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_FINGER_SCAN_START, "add_on_finger_scan_start_callback"
    ),
    automation.CallbackAutomation(
        CONF_ON_FINGER_SCAN_MATCHED,
        "add_on_finger_scan_matched_callback",
        [(cg.uint16, "finger_id"), (cg.uint16, "confidence")],
    ),
    automation.CallbackAutomation(
        CONF_ON_FINGER_SCAN_UNMATCHED,
        "add_on_finger_scan_unmatched_callback",
    ),
    automation.CallbackAutomation(
        CONF_ON_FINGER_SCAN_MISPLACED,
        "add_on_finger_scan_misplaced_callback",
    ),
    automation.CallbackAutomation(
        CONF_ON_FINGER_SCAN_INVALID, "add_on_finger_scan_invalid_callback"
    ),
    automation.CallbackAutomation(
        CONF_ON_ENROLLMENT_SCAN,
        "add_on_enrollment_scan_callback",
        [(cg.uint8, "scan_num"), (cg.uint16, "finger_id")],
    ),
    automation.CallbackAutomation(
        CONF_ON_ENROLLMENT_DONE,
        "add_on_enrollment_done_callback",
        [(cg.uint16, "finger_id")],
    ),
    automation.CallbackAutomation(
        CONF_ON_ENROLLMENT_FAILED,
        "add_on_enrollment_failed_callback",
        [(cg.uint16, "finger_id")],
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_PASSWORD in config:
        password = config[CONF_PASSWORD]
        cg.add(var.set_password(password))
    await uart.register_uart_device(var, config)

    if CONF_NEW_PASSWORD in config:
        new_password = config[CONF_NEW_PASSWORD]
        cg.add(var.set_new_password(new_password))

    if CONF_SENSING_PIN in config:
        sensing_pin = await cg.gpio_pin_expression(config[CONF_SENSING_PIN])
        cg.add(var.set_sensing_pin(sensing_pin))

    if CONF_SENSOR_POWER_PIN in config:
        sensor_power_pin = await cg.gpio_pin_expression(config[CONF_SENSOR_POWER_PIN])
        cg.add(var.set_sensor_power_pin(sensor_power_pin))

    if CONF_IDLE_PERIOD_TO_SLEEP in config:
        idle_period_to_sleep_ms = config[CONF_IDLE_PERIOD_TO_SLEEP]
        cg.add(var.set_idle_period_to_sleep_ms(idle_period_to_sleep_ms))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


@automation.register_action(
    "fingerprint_grow.enroll",
    EnrollmentAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(FingerprintGrowComponent),
            cv.Required(CONF_FINGER_ID): cv.templatable(cv.uint16_t),
            cv.Optional(CONF_NUM_SCANS): cv.templatable(cv.uint8_t),
        },
        key=CONF_FINGER_ID,
    ),
    synchronous=True,
)
async def fingerprint_grow_enroll_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_ = await cg.templatable(config[CONF_FINGER_ID], args, cg.uint16)
    cg.add(var.set_finger_id(template_))
    if CONF_NUM_SCANS in config:
        template_ = await cg.templatable(config[CONF_NUM_SCANS], args, cg.uint8)
        cg.add(var.set_num_scans(template_))
    return var


@automation.register_action(
    "fingerprint_grow.cancel_enroll",
    CancelEnrollmentAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(FingerprintGrowComponent),
        }
    ),
    synchronous=True,
)
async def fingerprint_grow_cancel_enroll_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "fingerprint_grow.delete",
    DeleteAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(FingerprintGrowComponent),
            cv.Required(CONF_FINGER_ID): cv.templatable(cv.uint16_t),
        },
        key=CONF_FINGER_ID,
    ),
    synchronous=True,
)
async def fingerprint_grow_delete_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_ = await cg.templatable(config[CONF_FINGER_ID], args, cg.uint16)
    cg.add(var.set_finger_id(template_))
    return var


@automation.register_action(
    "fingerprint_grow.delete_all",
    DeleteAllAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(FingerprintGrowComponent),
        }
    ),
    synchronous=True,
)
async def fingerprint_grow_delete_all_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


FINGERPRINT_GROW_LED_CONTROL_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(FingerprintGrowComponent),
        cv.Required(CONF_STATE): cv.templatable(cv.boolean),
    },
    key=CONF_STATE,
)


@automation.register_action(
    "fingerprint_grow.led_control",
    LEDControlAction,
    FINGERPRINT_GROW_LED_CONTROL_ACTION_SCHEMA,
    synchronous=True,
)
async def fingerprint_grow_led_control_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_ = await cg.templatable(config[CONF_STATE], args, cg.bool_)
    cg.add(var.set_state(template_))
    return var


@automation.register_action(
    "fingerprint_grow.aura_led_control",
    AuraLEDControlAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(FingerprintGrowComponent),
            cv.Required(CONF_STATE): cv.templatable(validate_aura_led_states),
            cv.Required(CONF_SPEED): cv.templatable(cv.uint8_t),
            cv.Required(CONF_COLOR): cv.templatable(validate_aura_led_colors),
            cv.Required(CONF_COUNT): cv.templatable(cv.uint8_t),
        }
    ),
    synchronous=True,
)
async def fingerprint_grow_aura_led_control_to_code(
    config, action_id, template_arg, args
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    for key in [CONF_STATE, CONF_SPEED, CONF_COLOR, CONF_COUNT]:
        template_ = await cg.templatable(config[key], args, cg.uint8)
        cg.add(getattr(var, f"set_{key}")(template_))
    return var
