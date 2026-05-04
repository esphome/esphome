from esphome import automation
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTION_STATE_TOPIC,
    CONF_AWAY,
    CONF_AWAY_COMMAND_TOPIC,
    CONF_AWAY_STATE_TOPIC,
    CONF_CURRENT_HUMIDITY_STATE_TOPIC,
    CONF_CURRENT_TEMPERATURE,
    CONF_CURRENT_TEMPERATURE_STATE_TOPIC,
    CONF_CUSTOM_FAN_MODE,
    CONF_CUSTOM_PRESET,
    CONF_ENTITY_CATEGORY,
    CONF_FAN_MODE,
    CONF_FAN_MODE_COMMAND_TOPIC,
    CONF_FAN_MODE_STATE_TOPIC,
    CONF_ICON,
    CONF_ID,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_MODE,
    CONF_MODE_COMMAND_TOPIC,
    CONF_MODE_STATE_TOPIC,
    CONF_MQTT_ID,
    CONF_ON_CONTROL,
    CONF_ON_STATE,
    CONF_PRESET,
    CONF_PRESET_COMMAND_TOPIC,
    CONF_PRESET_STATE_TOPIC,
    CONF_SWING_MODE,
    CONF_SWING_MODE_COMMAND_TOPIC,
    CONF_SWING_MODE_STATE_TOPIC,
    CONF_TARGET_HUMIDITY_COMMAND_TOPIC,
    CONF_TARGET_HUMIDITY_STATE_TOPIC,
    CONF_TARGET_TEMPERATURE,
    CONF_TARGET_TEMPERATURE_COMMAND_TOPIC,
    CONF_TARGET_TEMPERATURE_HIGH,
    CONF_TARGET_TEMPERATURE_HIGH_COMMAND_TOPIC,
    CONF_TARGET_TEMPERATURE_HIGH_STATE_TOPIC,
    CONF_TARGET_TEMPERATURE_LOW,
    CONF_TARGET_TEMPERATURE_LOW_COMMAND_TOPIC,
    CONF_TARGET_TEMPERATURE_LOW_STATE_TOPIC,
    CONF_TARGET_TEMPERATURE_STATE_TOPIC,
    CONF_TEMPERATURE_STEP,
    CONF_TRIGGER_ID,
    CONF_VISUAL,
    CONF_WEB_SERVER,
)
from esphome.core import CORE, CoroPriority, Lambda, coroutine_with_priority
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    queue_entity_register,
    setup_entity,
)
from esphome.cpp_generator import LambdaExpression, MockObjClass

IS_PLATFORM_COMPONENT = True

CODEOWNERS = ["@esphome/core"]
climate_ns = cg.esphome_ns.namespace("climate")

Climate = climate_ns.class_("Climate", cg.EntityBase)
ClimateCall = climate_ns.class_("ClimateCall")
ClimateTraits = climate_ns.class_("ClimateTraits")

ClimateMode = climate_ns.enum("ClimateMode")
CLIMATE_MODES = {
    "OFF": ClimateMode.CLIMATE_MODE_OFF,
    "HEAT_COOL": ClimateMode.CLIMATE_MODE_HEAT_COOL,
    "COOL": ClimateMode.CLIMATE_MODE_COOL,
    "HEAT": ClimateMode.CLIMATE_MODE_HEAT,
    "DRY": ClimateMode.CLIMATE_MODE_DRY,
    "FAN_ONLY": ClimateMode.CLIMATE_MODE_FAN_ONLY,
    "AUTO": ClimateMode.CLIMATE_MODE_AUTO,
}
validate_climate_mode = cv.enum(CLIMATE_MODES, upper=True)

ClimateFanMode = climate_ns.enum("ClimateFanMode")
CLIMATE_FAN_MODES = {
    "ON": ClimateFanMode.CLIMATE_FAN_ON,
    "OFF": ClimateFanMode.CLIMATE_FAN_OFF,
    "AUTO": ClimateFanMode.CLIMATE_FAN_AUTO,
    "LOW": ClimateFanMode.CLIMATE_FAN_LOW,
    "MEDIUM": ClimateFanMode.CLIMATE_FAN_MEDIUM,
    "HIGH": ClimateFanMode.CLIMATE_FAN_HIGH,
    "MIDDLE": ClimateFanMode.CLIMATE_FAN_MIDDLE,
    "FOCUS": ClimateFanMode.CLIMATE_FAN_FOCUS,
    "DIFFUSE": ClimateFanMode.CLIMATE_FAN_DIFFUSE,
    "QUIET": ClimateFanMode.CLIMATE_FAN_QUIET,
}

validate_climate_fan_mode = cv.enum(CLIMATE_FAN_MODES, upper=True)

ClimatePreset = climate_ns.enum("ClimatePreset")
CLIMATE_PRESETS = {
    "NONE": ClimatePreset.CLIMATE_PRESET_NONE,
    "ECO": ClimatePreset.CLIMATE_PRESET_ECO,
    "AWAY": ClimatePreset.CLIMATE_PRESET_AWAY,
    "BOOST": ClimatePreset.CLIMATE_PRESET_BOOST,
    "COMFORT": ClimatePreset.CLIMATE_PRESET_COMFORT,
    "HOME": ClimatePreset.CLIMATE_PRESET_HOME,
    "SLEEP": ClimatePreset.CLIMATE_PRESET_SLEEP,
    "ACTIVITY": ClimatePreset.CLIMATE_PRESET_ACTIVITY,
}

validate_climate_preset = cv.enum(CLIMATE_PRESETS, upper=True)

ClimateSwingMode = climate_ns.enum("ClimateSwingMode")
CLIMATE_SWING_MODES = {
    "OFF": ClimateSwingMode.CLIMATE_SWING_OFF,
    "BOTH": ClimateSwingMode.CLIMATE_SWING_BOTH,
    "VERTICAL": ClimateSwingMode.CLIMATE_SWING_VERTICAL,
    "HORIZONTAL": ClimateSwingMode.CLIMATE_SWING_HORIZONTAL,
}

validate_climate_swing_mode = cv.enum(CLIMATE_SWING_MODES, upper=True)

CONF_MIN_HUMIDITY = "min_humidity"
CONF_MAX_HUMIDITY = "max_humidity"
CONF_TARGET_HUMIDITY = "target_humidity"

visual_temperature = cv.float_with_unit("visual_temperature", "(°|(° ?)?[CKF])?")


VISUAL_TEMPERATURE_STEP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TARGET_TEMPERATURE): visual_temperature,
        cv.Required(CONF_CURRENT_TEMPERATURE): visual_temperature,
    }
)


def visual_temperature_step(value):
    # Allow defining target/current temperature steps separately
    if isinstance(value, dict):
        return VISUAL_TEMPERATURE_STEP_SCHEMA(value)

    # Otherwise, use the single value for both properties
    value = visual_temperature(value)
    return VISUAL_TEMPERATURE_STEP_SCHEMA(
        {
            CONF_TARGET_TEMPERATURE: value,
            CONF_CURRENT_TEMPERATURE: value,
        }
    )


# Actions
ControlAction = climate_ns.class_("ControlAction", automation.Action)
StateTrigger = climate_ns.class_(
    "StateTrigger", automation.Trigger.template(Climate.operator("ref"))
)
ControlTrigger = climate_ns.class_(
    "ControlTrigger", automation.Trigger.template(ClimateCall.operator("ref"))
)

_CLIMATE_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTClimateComponent),
            cv.Optional(CONF_VISUAL, default={}): cv.Schema(
                {
                    cv.Optional(CONF_MIN_TEMPERATURE): cv.temperature,
                    cv.Optional(CONF_MAX_TEMPERATURE): cv.temperature,
                    cv.Optional(CONF_TEMPERATURE_STEP): visual_temperature_step,
                    cv.Optional(CONF_MIN_HUMIDITY): cv.percentage_int,
                    cv.Optional(CONF_MAX_HUMIDITY): cv.percentage_int,
                }
            ),
            cv.Optional(CONF_ACTION_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_AWAY_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_AWAY_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_CURRENT_TEMPERATURE_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_CURRENT_HUMIDITY_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_FAN_MODE_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_FAN_MODE_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_MODE_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_MODE_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_PRESET_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_PRESET_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_SWING_MODE_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_SWING_MODE_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_TARGET_TEMPERATURE_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_TARGET_TEMPERATURE_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_TARGET_TEMPERATURE_HIGH_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_TARGET_TEMPERATURE_HIGH_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_TARGET_TEMPERATURE_LOW_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_TARGET_TEMPERATURE_LOW_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_TARGET_HUMIDITY_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_TARGET_HUMIDITY_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_ON_CONTROL): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ControlTrigger),
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


_CLIMATE_SCHEMA.add_extra(entity_duplicate_validator("climate"))


def climate_schema(
    class_: MockObjClass,
    *,
    entity_category: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
) -> cv.Schema:
    schema = {
        cv.GenerateID(): cv.declare_id(class_),
    }

    for key, default, validator in [
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_ICON, icon, cv.icon),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _CLIMATE_SCHEMA.extend(schema)


@setup_entity("climate")
async def setup_climate_core_(var, config):
    visual = config[CONF_VISUAL]
    if (min_temp := visual.get(CONF_MIN_TEMPERATURE)) is not None:
        cg.add_define("USE_CLIMATE_VISUAL_OVERRIDES")
        cg.add(var.set_visual_min_temperature_override(min_temp))
    if (max_temp := visual.get(CONF_MAX_TEMPERATURE)) is not None:
        cg.add_define("USE_CLIMATE_VISUAL_OVERRIDES")
        cg.add(var.set_visual_max_temperature_override(max_temp))
    if (temp_step := visual.get(CONF_TEMPERATURE_STEP)) is not None:
        cg.add_define("USE_CLIMATE_VISUAL_OVERRIDES")
        cg.add(
            var.set_visual_temperature_step_override(
                temp_step[CONF_TARGET_TEMPERATURE],
                temp_step[CONF_CURRENT_TEMPERATURE],
            )
        )
    if (min_humidity := visual.get(CONF_MIN_HUMIDITY)) is not None:
        cg.add_define("USE_CLIMATE_VISUAL_OVERRIDES")
        cg.add(var.set_visual_min_humidity_override(min_humidity))
    if (max_humidity := visual.get(CONF_MAX_HUMIDITY)) is not None:
        cg.add_define("USE_CLIMATE_VISUAL_OVERRIDES")
        cg.add(var.set_visual_max_humidity_override(max_humidity))

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

        if (action_state_topic := config.get(CONF_ACTION_STATE_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_action_state_topic(action_state_topic))
        if (away_command_topic := config.get(CONF_AWAY_COMMAND_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_away_command_topic(away_command_topic))
        if (away_state_topic := config.get(CONF_AWAY_STATE_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_away_state_topic(away_state_topic))
        if (
            current_temperature_state_topic := config.get(
                CONF_CURRENT_TEMPERATURE_STATE_TOPIC
            )
        ) is not None:
            cg.add(
                mqtt_.set_custom_current_temperature_state_topic(
                    current_temperature_state_topic
                )
            )
        if (
            current_humidity_state_topic := config.get(
                CONF_CURRENT_HUMIDITY_STATE_TOPIC
            )
        ) is not None:
            cg.add(
                mqtt_.set_custom_current_humidity_state_topic(
                    current_humidity_state_topic
                )
            )
        if (
            fan_mode_command_topic := config.get(CONF_FAN_MODE_COMMAND_TOPIC)
        ) is not None:
            cg.add(mqtt_.set_custom_fan_mode_command_topic(fan_mode_command_topic))
        if (fan_mode_state_topic := config.get(CONF_FAN_MODE_STATE_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_fan_mode_state_topic(fan_mode_state_topic))
        if (mode_command_topic := config.get(CONF_MODE_COMMAND_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_mode_command_topic(mode_command_topic))
        if (mode_state_topic := config.get(CONF_MODE_STATE_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_mode_state_topic(mode_state_topic))
        if (preset_command_topic := config.get(CONF_PRESET_COMMAND_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_preset_command_topic(preset_command_topic))
        if (preset_state_topic := config.get(CONF_PRESET_STATE_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_preset_state_topic(preset_state_topic))
        if (
            swing_mode_command_topic := config.get(CONF_SWING_MODE_COMMAND_TOPIC)
        ) is not None:
            cg.add(mqtt_.set_custom_swing_mode_command_topic(swing_mode_command_topic))
        if (
            swing_mode_state_topic := config.get(CONF_SWING_MODE_STATE_TOPIC)
        ) is not None:
            cg.add(mqtt_.set_custom_swing_mode_state_topic(swing_mode_state_topic))
        if (
            target_temperature_command_topic := config.get(
                CONF_TARGET_TEMPERATURE_COMMAND_TOPIC
            )
        ) is not None:
            cg.add(
                mqtt_.set_custom_target_temperature_command_topic(
                    target_temperature_command_topic
                )
            )
        if (
            target_temperature_state_topic := config.get(
                CONF_TARGET_TEMPERATURE_STATE_TOPIC
            )
        ) is not None:
            cg.add(
                mqtt_.set_custom_target_temperature_state_topic(
                    target_temperature_state_topic
                )
            )
        if (
            target_temperature_high_command_topic := config.get(
                CONF_TARGET_TEMPERATURE_HIGH_COMMAND_TOPIC
            )
        ) is not None:
            cg.add(
                mqtt_.set_custom_target_temperature_high_command_topic(
                    target_temperature_high_command_topic
                )
            )
        if (
            target_temperature_high_state_topic := config.get(
                CONF_TARGET_TEMPERATURE_HIGH_STATE_TOPIC
            )
        ) is not None:
            cg.add(
                mqtt_.set_custom_target_temperature_high_state_topic(
                    target_temperature_high_state_topic
                )
            )
        if (
            target_temperature_low_command_topic := config.get(
                CONF_TARGET_TEMPERATURE_LOW_COMMAND_TOPIC
            )
        ) is not None:
            cg.add(
                mqtt_.set_custom_target_temperature_low_command_topic(
                    target_temperature_low_command_topic
                )
            )
        if (
            target_temperature_low_state_topic := config.get(
                CONF_TARGET_TEMPERATURE_LOW_STATE_TOPIC
            )
        ) is not None:
            cg.add(
                mqtt_.set_custom_target_temperature_low_state_topic(
                    target_temperature_low_state_topic
                )
            )
        if (
            target_humidity_command_topic := config.get(
                CONF_TARGET_HUMIDITY_COMMAND_TOPIC
            )
        ) is not None:
            cg.add(
                mqtt_.set_custom_target_humidity_command_topic(
                    target_humidity_command_topic
                )
            )
        if (
            target_humidity_state_topic := config.get(CONF_TARGET_HUMIDITY_STATE_TOPIC)
        ) is not None:
            cg.add(
                mqtt_.set_custom_target_humidity_state_topic(
                    target_humidity_state_topic
                )
            )

    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(Climate.operator("ref"), "x")], conf
        )

    for conf in config.get(CONF_ON_CONTROL, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(ClimateCall.operator("ref"), "x")], conf
        )

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)


async def register_climate(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("climate", config)
    CORE.register_platform_component("climate", var)
    await setup_climate_core_(var, config)


async def new_climate(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_climate(var, config)
    return var


CLIMATE_CONTROL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Climate),
        cv.Optional(CONF_MODE): cv.templatable(validate_climate_mode),
        cv.Optional(CONF_TARGET_TEMPERATURE): cv.templatable(cv.temperature),
        cv.Optional(CONF_TARGET_TEMPERATURE_LOW): cv.templatable(cv.temperature),
        cv.Optional(CONF_TARGET_TEMPERATURE_HIGH): cv.templatable(cv.temperature),
        cv.Optional(CONF_TARGET_HUMIDITY): cv.templatable(cv.percentage_int),
        cv.Optional(CONF_AWAY): cv.invalid("Use preset instead"),
        cv.Exclusive(CONF_FAN_MODE, "fan_mode"): cv.templatable(
            validate_climate_fan_mode
        ),
        cv.Exclusive(CONF_CUSTOM_FAN_MODE, "fan_mode"): cv.templatable(
            cv.string_strict
        ),
        cv.Exclusive(CONF_PRESET, "preset"): cv.templatable(validate_climate_preset),
        cv.Exclusive(CONF_CUSTOM_PRESET, "preset"): cv.templatable(cv.string_strict),
        cv.Optional(CONF_SWING_MODE): cv.templatable(validate_climate_swing_mode),
    }
)


@automation.register_action(
    "climate.control",
    ControlAction,
    CLIMATE_CONTROL_ACTION_SCHEMA,
    synchronous=True,
)
async def climate_control_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])

    # All configured fields are folded into a single stateless lambda whose
    # constants live in flash; the action stores only a function pointer.
    # For custom_fan_mode/custom_preset the static-string path emits the
    # (const char *, size_t) overload of set_fan_mode/set_preset to avoid
    # constructing a std::string and calling runtime strlen.
    FIELDS = (
        (CONF_MODE, "set_mode", ClimateMode),
        (CONF_TARGET_TEMPERATURE, "set_target_temperature", cg.float_),
        (CONF_TARGET_TEMPERATURE_LOW, "set_target_temperature_low", cg.float_),
        (CONF_TARGET_TEMPERATURE_HIGH, "set_target_temperature_high", cg.float_),
        (CONF_TARGET_HUMIDITY, "set_target_humidity", cg.float_),
        (CONF_FAN_MODE, "set_fan_mode", ClimateFanMode),
        (CONF_CUSTOM_FAN_MODE, "set_fan_mode", cg.std_string),
        (CONF_PRESET, "set_preset", ClimatePreset),
        (CONF_CUSTOM_PRESET, "set_preset", cg.std_string),
        (CONF_SWING_MODE, "set_swing_mode", ClimateSwingMode),
    )

    # Normalize trigger args to `const std::remove_cvref_t<T> &` so the
    # apply lambda and any inner field lambdas (generated below via
    # `process_lambda`) share one parameter spelling that's well-formed for
    # any T (value, ref, or const-ref). Matches ControlAction::ApplyFn.
    normalized_args = [
        (cg.RawExpression(f"const std::remove_cvref_t<{cg.safe_exp(t)}> &"), n)
        for t, n in args
    ]

    fwd_args = ", ".join(name for _, name in args)
    body_lines: list[str] = []

    for conf_key, setter, type_ in FIELDS:
        if (value := config.get(conf_key)) is None:
            continue
        if isinstance(value, Lambda):
            inner = await cg.process_lambda(value, normalized_args, return_type=type_)
            body_lines.append(f"call.{setter}(({inner})({fwd_args}));")
        elif type_ is cg.std_string:
            # Static custom strings: emit a flash literal and pass the
            # UTF-8 byte length to skip the runtime strlen inside
            # set_fan_mode/set_preset.
            literal = cg.safe_exp(value)
            body_lines.append(
                f"call.{setter}({literal}, {len(value.encode('utf-8'))});"
            )
        else:
            body_lines.append(f"call.{setter}({cg.safe_exp(value)});")

    apply_args = [
        (ClimateCall.operator("ref"), "call"),
        *normalized_args,
    ]
    apply_lambda = LambdaExpression(
        ["\n".join(body_lines)],
        apply_args,
        capture="",
        return_type=cg.void,
    )
    return cg.new_Pvariable(action_id, template_arg, paren, apply_lambda)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(climate_ns.using)
