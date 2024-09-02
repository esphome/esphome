from esphome import automation, core
from esphome.automation import Condition, maybe_simple_id
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_DELAY,
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_FILTERS,
    CONF_ICON,
    CONF_ID,
    CONF_INVALID_COOLDOWN,
    CONF_INVERTED,
    CONF_MAX_LENGTH,
    CONF_MIN_LENGTH,
    CONF_MQTT_ID,
    CONF_ON_CLICK,
    CONF_ON_DOUBLE_CLICK,
    CONF_ON_MULTI_CLICK,
    CONF_ON_PRESS,
    CONF_ON_RELEASE,
    CONF_ON_STATE,
    CONF_PUBLISH_INITIAL_STATE,
    CONF_STATE,
    CONF_TIMING,
    CONF_TRIGGER_ID,
    CONF_WEB_SERVER_ID,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_COLD,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_DOOR,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_GARAGE_DOOR,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_LIGHT,
    DEVICE_CLASS_LOCK,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_MOVING,
    DEVICE_CLASS_OCCUPANCY,
    DEVICE_CLASS_OPENING,
    DEVICE_CLASS_PLUG,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PRESENCE,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_RUNNING,
    DEVICE_CLASS_SAFETY,
    DEVICE_CLASS_SMOKE,
    DEVICE_CLASS_SOUND,
    DEVICE_CLASS_TAMPER,
    DEVICE_CLASS_UPDATE,
    DEVICE_CLASS_VIBRATION,
    DEVICE_CLASS_WINDOW,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_generator import MockObjClass
from esphome.cpp_helpers import setup_entity
from esphome.util import Registry

CODEOWNERS = ["@esphome/core"]
DEVICE_CLASSES = [
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_COLD,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_DOOR,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_GARAGE_DOOR,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_LIGHT,
    DEVICE_CLASS_LOCK,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_MOVING,
    DEVICE_CLASS_OCCUPANCY,
    DEVICE_CLASS_OPENING,
    DEVICE_CLASS_PLUG,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PRESENCE,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_RUNNING,
    DEVICE_CLASS_SAFETY,
    DEVICE_CLASS_SMOKE,
    DEVICE_CLASS_SOUND,
    DEVICE_CLASS_TAMPER,
    DEVICE_CLASS_UPDATE,
    DEVICE_CLASS_VIBRATION,
    DEVICE_CLASS_WINDOW,
]

IS_PLATFORM_COMPONENT = True

CONF_TIME_OFF = "time_off"
CONF_TIME_ON = "time_on"

DEFAULT_DELAY = "1s"
DEFAULT_TIME_OFF = "100ms"
DEFAULT_TIME_ON = "900ms"


binary_sensor_ns = cg.esphome_ns.namespace("binary_sensor")
BinarySensor = binary_sensor_ns.class_("BinarySensor", cg.EntityBase)
BinarySensorInitiallyOff = binary_sensor_ns.class_(
    "BinarySensorInitiallyOff", BinarySensor
)
BinarySensorPtr = BinarySensor.operator("ptr")

# Triggers
PressTrigger = binary_sensor_ns.class_("PressTrigger", automation.Trigger.template())
ReleaseTrigger = binary_sensor_ns.class_(
    "ReleaseTrigger", automation.Trigger.template()
)
ClickTrigger = binary_sensor_ns.class_("ClickTrigger", automation.Trigger.template())
DoubleClickTrigger = binary_sensor_ns.class_(
    "DoubleClickTrigger", automation.Trigger.template()
)
MultiClickTrigger = binary_sensor_ns.class_(
    "MultiClickTrigger", automation.Trigger.template(), cg.Component
)
MultiClickTriggerEvent = binary_sensor_ns.struct("MultiClickTriggerEvent")
StateTrigger = binary_sensor_ns.class_(
    "StateTrigger", automation.Trigger.template(bool)
)
BinarySensorPublishAction = binary_sensor_ns.class_(
    "BinarySensorPublishAction", automation.Action
)

# Condition
BinarySensorCondition = binary_sensor_ns.class_("BinarySensorCondition", Condition)

# Filters
Filter = binary_sensor_ns.class_("Filter")
DelayedOnOffFilter = binary_sensor_ns.class_("DelayedOnOffFilter", Filter, cg.Component)
DelayedOnFilter = binary_sensor_ns.class_("DelayedOnFilter", Filter, cg.Component)
DelayedOffFilter = binary_sensor_ns.class_("DelayedOffFilter", Filter, cg.Component)
InvertFilter = binary_sensor_ns.class_("InvertFilter", Filter)
AutorepeatFilter = binary_sensor_ns.class_("AutorepeatFilter", Filter, cg.Component)
LambdaFilter = binary_sensor_ns.class_("LambdaFilter", Filter)
SettleFilter = binary_sensor_ns.class_("SettleFilter", Filter, cg.Component)

FILTER_REGISTRY = Registry()
validate_filters = cv.validate_registry("filter", FILTER_REGISTRY)


def register_filter(name, filter_type, schema):
    return FILTER_REGISTRY.register(name, filter_type, schema)


@register_filter("invert", InvertFilter, {})
async def invert_filter_to_code(config, filter_id):
    return cg.new_Pvariable(filter_id)


@register_filter(
    "delayed_on_off",
    DelayedOnOffFilter,
    cv.Any(
        cv.templatable(cv.positive_time_period_milliseconds),
        cv.Schema(
            {
                cv.Required(CONF_TIME_ON): cv.templatable(
                    cv.positive_time_period_milliseconds
                ),
                cv.Required(CONF_TIME_OFF): cv.templatable(
                    cv.positive_time_period_milliseconds
                ),
            }
        ),
        msg="'delayed_on_off' filter requires either a delay time to be used for both "
        "turn-on and turn-off delays, or two parameters 'time_on' and 'time_off' if "
        "different delay times are required.",
    ),
)
async def delayed_on_off_filter_to_code(config, filter_id):
    var = cg.new_Pvariable(filter_id)
    await cg.register_component(var, {})
    if isinstance(config, dict):
        template_ = await cg.templatable(config[CONF_TIME_ON], [], cg.uint32)
        cg.add(var.set_on_delay(template_))
        template_ = await cg.templatable(config[CONF_TIME_OFF], [], cg.uint32)
        cg.add(var.set_off_delay(template_))
    else:
        template_ = await cg.templatable(config, [], cg.uint32)
        cg.add(var.set_on_delay(template_))
        cg.add(var.set_off_delay(template_))
    return var


@register_filter(
    "delayed_on", DelayedOnFilter, cv.templatable(cv.positive_time_period_milliseconds)
)
async def delayed_on_filter_to_code(config, filter_id):
    var = cg.new_Pvariable(filter_id)
    await cg.register_component(var, {})
    template_ = await cg.templatable(config, [], cg.uint32)
    cg.add(var.set_delay(template_))
    return var


@register_filter(
    "delayed_off",
    DelayedOffFilter,
    cv.templatable(cv.positive_time_period_milliseconds),
)
async def delayed_off_filter_to_code(config, filter_id):
    var = cg.new_Pvariable(filter_id)
    await cg.register_component(var, {})
    template_ = await cg.templatable(config, [], cg.uint32)
    cg.add(var.set_delay(template_))
    return var


@register_filter(
    "autorepeat",
    AutorepeatFilter,
    cv.All(
        cv.ensure_list(
            {
                cv.Optional(
                    CONF_DELAY, default=DEFAULT_DELAY
                ): cv.positive_time_period_milliseconds,
                cv.Optional(
                    CONF_TIME_OFF, default=DEFAULT_TIME_OFF
                ): cv.positive_time_period_milliseconds,
                cv.Optional(
                    CONF_TIME_ON, default=DEFAULT_TIME_ON
                ): cv.positive_time_period_milliseconds,
            }
        ),
    ),
)
async def autorepeat_filter_to_code(config, filter_id):
    timings = []
    if len(config) > 0:
        for conf in config:
            timings.append((conf[CONF_DELAY], conf[CONF_TIME_OFF], conf[CONF_TIME_ON]))
    else:
        timings.append(
            (
                cv.time_period_str_unit(DEFAULT_DELAY).total_milliseconds,
                cv.time_period_str_unit(DEFAULT_TIME_OFF).total_milliseconds,
                cv.time_period_str_unit(DEFAULT_TIME_ON).total_milliseconds,
            )
        )
    var = cg.new_Pvariable(filter_id, timings)
    await cg.register_component(var, {})
    return var


@register_filter("lambda", LambdaFilter, cv.returning_lambda)
async def lambda_filter_to_code(config, filter_id):
    lambda_ = await cg.process_lambda(
        config, [(bool, "x")], return_type=cg.optional.template(bool)
    )
    return cg.new_Pvariable(filter_id, lambda_)


@register_filter(
    "settle",
    SettleFilter,
    cv.templatable(cv.positive_time_period_milliseconds),
)
async def settle_filter_to_code(config, filter_id):
    var = cg.new_Pvariable(filter_id)
    await cg.register_component(var, {})
    template_ = await cg.templatable(config, [], cg.uint32)
    cg.add(var.set_delay(template_))
    return var


MULTI_CLICK_TIMING_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_STATE): cv.boolean,
        cv.Optional(CONF_MIN_LENGTH): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_LENGTH): cv.positive_time_period_milliseconds,
    }
)


def parse_multi_click_timing_str(value):
    if not isinstance(value, str):
        return value

    parts = value.lower().split(" ")
    if len(parts) != 5:
        raise cv.Invalid(
            f"Multi click timing grammar consists of exactly 5 words, not {len(parts)}"
        )
    try:
        state = cv.boolean(parts[0])
    except cv.Invalid:
        # pylint: disable=raise-missing-from
        raise cv.Invalid(f"First word must either be ON or OFF, not {parts[0]}")

    if parts[1] != "for":
        raise cv.Invalid(f"Second word must be 'for', got {parts[1]}")

    if parts[2] == "at":
        if parts[3] == "least":
            key = CONF_MIN_LENGTH
        elif parts[3] == "most":
            key = CONF_MAX_LENGTH
        else:
            raise cv.Invalid(
                f"Third word after at must either be 'least' or 'most', got {parts[3]}"
            )
        try:
            length = cv.positive_time_period_milliseconds(parts[4])
        except cv.Invalid as err:
            raise cv.Invalid(f"Multi Click Grammar Parsing length failed: {err}")
        return {CONF_STATE: state, key: str(length)}

    if parts[3] != "to":
        raise cv.Invalid("Multi click grammar: 4th word must be 'to'")

    try:
        min_length = cv.positive_time_period_milliseconds(parts[2])
    except cv.Invalid as err:
        raise cv.Invalid(f"Multi Click Grammar Parsing minimum length failed: {err}")

    try:
        max_length = cv.positive_time_period_milliseconds(parts[4])
    except cv.Invalid as err:
        raise cv.Invalid(f"Multi Click Grammar Parsing minimum length failed: {err}")

    return {
        CONF_STATE: state,
        CONF_MIN_LENGTH: str(min_length),
        CONF_MAX_LENGTH: str(max_length),
    }


def validate_multi_click_timing(value):
    if not isinstance(value, list):
        raise cv.Invalid("Timing option must be a *list* of times!")
    timings = []
    state = None
    for i, v_ in enumerate(value):
        v_ = MULTI_CLICK_TIMING_SCHEMA(v_)
        min_length = v_.get(CONF_MIN_LENGTH)
        max_length = v_.get(CONF_MAX_LENGTH)
        if min_length is None and max_length is None:
            raise cv.Invalid("At least one of min_length and max_length is required!")
        if min_length is None and max_length is not None:
            min_length = core.TimePeriodMilliseconds(milliseconds=0)

        new_state = v_.get(CONF_STATE, not state)
        if new_state == state:
            raise cv.Invalid(
                f"Timings must have alternating state. Indices {i} and {i + 1} have the same state {state}"
            )
        if max_length is not None and max_length < min_length:
            raise cv.Invalid(
                f"Max length ({max_length}) must be larger than min length ({min_length})."
            )

        state = new_state
        tim = {
            CONF_STATE: new_state,
            CONF_MIN_LENGTH: min_length,
        }
        if max_length is not None:
            tim[CONF_MAX_LENGTH] = max_length
        timings.append(tim)
    return timings


validate_device_class = cv.one_of(*DEVICE_CLASSES, lower=True, space="_")


def validate_click_timing(value):
    for v in value:
        min_length = v.get(CONF_MIN_LENGTH)
        max_length = v.get(CONF_MAX_LENGTH)
        if max_length < min_length:
            raise cv.Invalid(
                f"Max length ({max_length}) must be larger than min length ({min_length})."
            )

    return value


BINARY_SENSOR_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMPONENT_SCHEMA)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(BinarySensor),
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt.MQTTBinarySensorComponent
            ),
            cv.Optional(CONF_PUBLISH_INITIAL_STATE): cv.boolean,
            cv.Optional(CONF_DEVICE_CLASS): validate_device_class,
            cv.Optional(CONF_FILTERS): validate_filters,
            cv.Optional(CONF_ON_PRESS): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PressTrigger),
                }
            ),
            cv.Optional(CONF_ON_RELEASE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ReleaseTrigger),
                }
            ),
            cv.Optional(CONF_ON_CLICK): cv.All(
                automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ClickTrigger),
                        cv.Optional(
                            CONF_MIN_LENGTH, default="50ms"
                        ): cv.positive_time_period_milliseconds,
                        cv.Optional(
                            CONF_MAX_LENGTH, default="350ms"
                        ): cv.positive_time_period_milliseconds,
                    }
                ),
                validate_click_timing,
            ),
            cv.Optional(CONF_ON_DOUBLE_CLICK): cv.All(
                automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            DoubleClickTrigger
                        ),
                        cv.Optional(
                            CONF_MIN_LENGTH, default="50ms"
                        ): cv.positive_time_period_milliseconds,
                        cv.Optional(
                            CONF_MAX_LENGTH, default="350ms"
                        ): cv.positive_time_period_milliseconds,
                    }
                ),
                validate_click_timing,
            ),
            cv.Optional(CONF_ON_MULTI_CLICK): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MultiClickTrigger),
                    cv.Required(CONF_TIMING): cv.All(
                        [parse_multi_click_timing_str], validate_multi_click_timing
                    ),
                    cv.Optional(
                        CONF_INVALID_COOLDOWN, default="1s"
                    ): cv.positive_time_period_milliseconds,
                }
            ),
            cv.Optional(CONF_ON_STATE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StateTrigger),
                }
            ),
        }
    )
)

_UNDEF = object()


def binary_sensor_schema(
    class_: MockObjClass = _UNDEF,
    *,
    icon: str = _UNDEF,
    entity_category: str = _UNDEF,
    device_class: str = _UNDEF,
) -> cv.Schema:
    schema = {}

    if class_ is not _UNDEF:
        # Not cv.optional
        schema[cv.GenerateID()] = cv.declare_id(class_)

    for key, default, validator in [
        (CONF_ICON, icon, cv.icon),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_DEVICE_CLASS, device_class, validate_device_class),
    ]:
        if default is not _UNDEF:
            schema[cv.Optional(key, default=default)] = validator

    return BINARY_SENSOR_SCHEMA.extend(schema)


async def setup_binary_sensor_core_(var, config):
    await setup_entity(var, config)

    if (device_class := config.get(CONF_DEVICE_CLASS)) is not None:
        cg.add(var.set_device_class(device_class))
    if publish_initial_state := config.get(CONF_PUBLISH_INITIAL_STATE):
        cg.add(var.set_publish_initial_state(publish_initial_state))
    if inverted := config.get(CONF_INVERTED):
        cg.add(var.set_inverted(inverted))
    if filters_config := config.get(CONF_FILTERS):
        filters = await cg.build_registry_list(FILTER_REGISTRY, filters_config)
        cg.add(var.add_filters(filters))

    for conf in config.get(CONF_ON_PRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_RELEASE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_CLICK, []):
        trigger = cg.new_Pvariable(
            conf[CONF_TRIGGER_ID], var, conf[CONF_MIN_LENGTH], conf[CONF_MAX_LENGTH]
        )
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_DOUBLE_CLICK, []):
        trigger = cg.new_Pvariable(
            conf[CONF_TRIGGER_ID], var, conf[CONF_MIN_LENGTH], conf[CONF_MAX_LENGTH]
        )
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_MULTI_CLICK, []):
        timings = []
        for tim in conf[CONF_TIMING]:
            timings.append(
                cg.StructInitializer(
                    MultiClickTriggerEvent,
                    ("state", tim[CONF_STATE]),
                    ("min_length", tim[CONF_MIN_LENGTH]),
                    ("max_length", tim.get(CONF_MAX_LENGTH, 4294967294)),
                )
            )
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var, timings)
        if CONF_INVALID_COOLDOWN in conf:
            cg.add(trigger.set_invalid_cooldown(conf[CONF_INVALID_COOLDOWN]))
        await cg.register_component(trigger, conf)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(bool, "x")], conf)

    if mqtt_id := config.get(CONF_MQTT_ID):
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if (webserver_id := config.get(CONF_WEB_SERVER_ID)) is not None:
        web_server_ = await cg.get_variable(webserver_id)
        web_server.add_entity_to_sorting_list(web_server_, var, config)


async def register_binary_sensor(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_binary_sensor(var))
    await setup_binary_sensor_core_(var, config)


async def new_binary_sensor(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_binary_sensor(var, config)
    return var


BINARY_SENSOR_CONDITION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(BinarySensor),
    }
)


@automation.register_condition(
    "binary_sensor.is_on", BinarySensorCondition, BINARY_SENSOR_CONDITION_SCHEMA
)
async def binary_sensor_is_on_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, True)


@automation.register_condition(
    "binary_sensor.is_off", BinarySensorCondition, BINARY_SENSOR_CONDITION_SCHEMA
)
async def binary_sensor_is_off_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, False)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_BINARY_SENSOR")
    cg.add_global(binary_sensor_ns.using)
