import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import humidifier, sensor
from esphome.const import (
    CONF_AUTO_MODE,
    CONF_AWAY_CONFIG,
    CONF_DEHUMIDIFY_ACTION,
    CONF_DEHUMIDIFY_DEADBAND,
    CONF_DEHUMIDIFY_MODE,
    CONF_DEHUMIDIFY_OVERRUN,
    CONF_DEFAULT_MODE,
    CONF_DEFAULT_TARGET_HUMIDITY_HIGH,
    CONF_DEFAULT_TARGET_HUMIDITY_LOW,
    CONF_HUMIDIFY_ACTION,
    CONF_HUMIDIFY_DEADBAND,
    CONF_HUMIDIFY_MODE,
    CONF_HUMIDIFY_OVERRUN,
    CONF_ID,
    CONF_IDLE_ACTION,
    CONF_MAX_DEHUMIDIFYING_RUN_TIME,
    CONF_MAX_HUMIDIFYING_RUN_TIME,
    CONF_MIN_DEHUMIDIFYING_OFF_TIME,
    CONF_MIN_DEHUMIDIFYING_RUN_TIME,
    CONF_MIN_HUMIDIFYING_OFF_TIME,
    CONF_MIN_HUMIDIFYING_RUN_TIME,
    CONF_MIN_IDLE_TIME,
    CONF_OFF_MODE,
    CONF_SENSOR,
    CONF_SET_POINT_MINIMUM_DIFFERENTIAL,
    CONF_STARTUP_DELAY,
    CONF_TARGET_HUMIDITY_CHANGE_ACTION,
)

CODEOWNERS = ["@alevaquero"]

humidifier_ns = cg.esphome_ns.namespace("humidifier")
hygrostat_ns = cg.esphome_ns.namespace("hygrostat")
HygrostatHumidifier = hygrostat_ns.class_(
    "HygrostatHumidifier", humidifier.Humidifier, cg.Component
)
HygrostatHumidifierTargetTempConfig = hygrostat_ns.struct(
    "HygrostatHumidifierTargetTempConfig"
)
HumidifierMode = humidifier_ns.enum("HumidifierMode")
HUMIDIFIER_MODES = {
    "OFF": HumidifierMode.HUMIDIFIER_MODE_OFF,
    "HUMIDIFY_DEHUMIDIFY": HumidifierMode.HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY,
    "HUMIDIFY": HumidifierMode.HUMIDIFIER_MODE_HUMIDIFY,
    "DEHUMIDIFY": HumidifierMode.HUMIDIFIER_MODE_DEHUMIDIFY,
    "AUTO": HumidifierMode.HUMIDIFIER_MODE_AUTO,
}
validate_humidifier_mode = cv.enum(HUMIDIFIER_MODES, upper=True)


def validate_hygrostat(config):
    # verify corresponding action(s) exist(s) for any defined humidifier mode or action
    requirements = {
        CONF_AUTO_MODE: [
            CONF_DEHUMIDIFY_ACTION,
            CONF_HUMIDIFY_ACTION,
            CONF_MIN_DEHUMIDIFYING_OFF_TIME,
            CONF_MIN_DEHUMIDIFYING_RUN_TIME,
            CONF_MIN_HUMIDIFYING_OFF_TIME,
            CONF_MIN_HUMIDIFYING_RUN_TIME,
        ],
        CONF_DEHUMIDIFY_MODE: [
            CONF_DEHUMIDIFY_ACTION,
            CONF_MIN_DEHUMIDIFYING_OFF_TIME,
            CONF_MIN_DEHUMIDIFYING_RUN_TIME,
        ],
        CONF_HUMIDIFY_MODE: [
            CONF_HUMIDIFY_ACTION,
            CONF_MIN_HUMIDIFYING_OFF_TIME,
            CONF_MIN_HUMIDIFYING_RUN_TIME,
        ],
        CONF_DEHUMIDIFY_ACTION: [
            CONF_MIN_DEHUMIDIFYING_OFF_TIME,
            CONF_MIN_DEHUMIDIFYING_RUN_TIME,
        ],
        CONF_HUMIDIFY_ACTION: [
            CONF_MIN_HUMIDIFYING_OFF_TIME,
            CONF_MIN_HUMIDIFYING_RUN_TIME,
        ],
        CONF_MAX_DEHUMIDIFYING_RUN_TIME: [
            CONF_DEHUMIDIFY_ACTION,
        ],
        CONF_MAX_HUMIDIFYING_RUN_TIME: [
            CONF_HUMIDIFY_ACTION,
        ],
        CONF_MIN_DEHUMIDIFYING_OFF_TIME: [
            CONF_DEHUMIDIFY_ACTION,
        ],
        CONF_MIN_DEHUMIDIFYING_RUN_TIME: [
            CONF_DEHUMIDIFY_ACTION,
        ],
        CONF_MIN_HUMIDIFYING_OFF_TIME: [
            CONF_HUMIDIFY_ACTION,
        ],
        CONF_MIN_HUMIDIFYING_RUN_TIME: [
            CONF_HUMIDIFY_ACTION,
        ],
    }
    for config_trigger, req_triggers in requirements.items():
        for req_trigger in req_triggers:
            if config_trigger in config and req_trigger not in config:
                raise cv.Invalid(
                    f"{req_trigger} must be defined to use {config_trigger}"
                )

    requirements = {
        CONF_DEFAULT_TARGET_HUMIDITY_HIGH: [CONF_DEHUMIDIFY_ACTION],
        CONF_DEFAULT_TARGET_HUMIDITY_LOW: [CONF_HUMIDIFY_ACTION],
    }

    for config_hum, req_actions in requirements.items():
        for req_action in req_actions:
            # verify corresponding default target humidity exists when a given humidifier action exists
            if config_hum not in config and req_action in config:
                raise cv.Invalid(
                    f"{config_hum} must be defined when using {req_action}"
                )
            # if a given humidifier action is NOT defined, it should not have a default target humidity
            if config_hum in config and req_action not in config:
                raise cv.Invalid(f"{config_hum} is defined with no {req_action}")

    if CONF_AWAY_CONFIG in config:
        away = config[CONF_AWAY_CONFIG]
        for config_hum, req_actions in requirements.items():
            for req_action in req_actions:
                # verify corresponding default target humidity exists when a given humidifier action exists
                if config_hum not in away and req_action in config:
                    raise cv.Invalid(
                        f"{config_hum} must be defined in away configuration when using {req_action}"
                    )
                # if a given humidifier action is NOT defined, it should not have a default target humidity
                if config_hum in away and req_action not in config:
                    raise cv.Invalid(
                        f"{config_hum} is defined in away configuration with no {req_action}"
                    )

    # verify default humidifier mode is valid given above configuration
    default_mode = config[CONF_DEFAULT_MODE]
    requirements = {
        "HUMIDIFY_DEHUMIDIFY": [CONF_DEHUMIDIFY_ACTION, CONF_HUMIDIFY_ACTION],
        "DEHUMIDIFY": [CONF_DEHUMIDIFY_ACTION],
        "HUMIDIFY": [CONF_HUMIDIFY_ACTION],
        "AUTO": [CONF_DEHUMIDIFY_ACTION, CONF_HUMIDIFY_ACTION],
    }.get(default_mode, [])
    for req in requirements:
        if req not in config:
            raise cv.Invalid(
                f"{CONF_DEFAULT_MODE} is set to {default_mode} but {req} is not present in the configuration"
            )

    return config


CONFIG_SCHEMA = cv.All(
    humidifier.HUMIDIFIER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HygrostatHumidifier),
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_IDLE_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_DEHUMIDIFY_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_HUMIDIFY_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_AUTO_MODE): automation.validate_automation(single=True),
            cv.Optional(CONF_DEHUMIDIFY_MODE): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_DEHUMIDIFY_MODE): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_HUMIDIFY_MODE): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_OFF_MODE): automation.validate_automation(single=True),
            cv.Optional(
                CONF_TARGET_HUMIDITY_CHANGE_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(CONF_DEFAULT_MODE, default="OFF"): cv.templatable(
                validate_humidifier_mode
            ),
            cv.Optional(CONF_DEFAULT_TARGET_HUMIDITY_HIGH): cv.humidity,
            cv.Optional(CONF_DEFAULT_TARGET_HUMIDITY_LOW): cv.humidity,
            cv.Optional(
                CONF_SET_POINT_MINIMUM_DIFFERENTIAL, default="0.5%"
            ): cv.humidity,
            cv.Optional(CONF_DEHUMIDIFY_DEADBAND, default="0.5%"): cv.humidity,
            cv.Optional(CONF_DEHUMIDIFY_OVERRUN, default="0.5%"): cv.humidity,
            cv.Optional(CONF_HUMIDIFY_DEADBAND, default="0.5%"): cv.humidity,
            cv.Optional(CONF_HUMIDIFY_OVERRUN, default="0.5%"): cv.humidity,
            cv.Optional(
                CONF_MAX_DEHUMIDIFYING_RUN_TIME
            ): cv.positive_time_period_seconds,
            cv.Optional(CONF_MAX_HUMIDIFYING_RUN_TIME): cv.positive_time_period_seconds,
            cv.Optional(
                CONF_MIN_DEHUMIDIFYING_OFF_TIME
            ): cv.positive_time_period_seconds,
            cv.Optional(
                CONF_MIN_DEHUMIDIFYING_RUN_TIME
            ): cv.positive_time_period_seconds,
            cv.Optional(CONF_MIN_HUMIDIFYING_OFF_TIME): cv.positive_time_period_seconds,
            cv.Optional(CONF_MIN_HUMIDIFYING_RUN_TIME): cv.positive_time_period_seconds,
            cv.Required(CONF_MIN_IDLE_TIME): cv.positive_time_period_seconds,
            cv.Optional(CONF_STARTUP_DELAY, default=False): cv.boolean,
            cv.Optional(CONF_AWAY_CONFIG): cv.Schema(
                {
                    cv.Optional(CONF_DEFAULT_TARGET_HUMIDITY_HIGH): cv.humidity,
                    cv.Optional(CONF_DEFAULT_TARGET_HUMIDITY_LOW): cv.humidity,
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_DEHUMIDIFY_ACTION, CONF_HUMIDIFY_ACTION),
    validate_hygrostat,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await humidifier.register_humidifier(var, config)

    humidify_dehumidify_mode_available = (
        CONF_HUMIDIFY_ACTION in config and CONF_DEHUMIDIFY_ACTION in config
    )

    sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_default_mode(config[CONF_DEFAULT_MODE]))
    cg.add(
        var.set_set_point_minimum_differential(
            config[CONF_SET_POINT_MINIMUM_DIFFERENTIAL]
        )
    )
    cg.add(var.set_sensor(sens))

    cg.add(var.set_dehumidify_deadband(config[CONF_DEHUMIDIFY_DEADBAND]))
    cg.add(var.set_dehumidify_overrun(config[CONF_DEHUMIDIFY_OVERRUN]))
    cg.add(var.set_humidify_deadband(config[CONF_HUMIDIFY_DEADBAND]))
    cg.add(var.set_humidify_overrun(config[CONF_HUMIDIFY_OVERRUN]))

    if humidify_dehumidify_mode_available is True:
        cg.add(var.set_supports_two_points(True))
        normal_config = HygrostatHumidifierTargetTempConfig(
            config[CONF_DEFAULT_TARGET_HUMIDITY_LOW],
            config[CONF_DEFAULT_TARGET_HUMIDITY_HIGH],
        )
    elif CONF_DEFAULT_TARGET_HUMIDITY_HIGH in config:
        cg.add(var.set_supports_two_points(False))
        normal_config = HygrostatHumidifierTargetTempConfig(
            config[CONF_DEFAULT_TARGET_HUMIDITY_HIGH]
        )
    elif CONF_DEFAULT_TARGET_HUMIDITY_LOW in config:
        cg.add(var.set_supports_two_points(False))
        normal_config = HygrostatHumidifierTargetTempConfig(
            config[CONF_DEFAULT_TARGET_HUMIDITY_LOW]
        )

    if CONF_MAX_DEHUMIDIFYING_RUN_TIME in config:
        cg.add(
            var.set_dehumidifying_maximum_run_time_in_sec(
                config[CONF_MAX_DEHUMIDIFYING_RUN_TIME]
            )
        )

    if CONF_MAX_HUMIDIFYING_RUN_TIME in config:
        cg.add(
            var.set_humidifying_maximum_run_time_in_sec(
                config[CONF_MAX_HUMIDIFYING_RUN_TIME]
            )
        )

    if CONF_MIN_DEHUMIDIFYING_OFF_TIME in config:
        cg.add(
            var.set_dehumidifying_minimum_off_time_in_sec(
                config[CONF_MIN_DEHUMIDIFYING_OFF_TIME]
            )
        )

    if CONF_MIN_DEHUMIDIFYING_RUN_TIME in config:
        cg.add(
            var.set_dehumidifying_minimum_run_time_in_sec(
                config[CONF_MIN_DEHUMIDIFYING_RUN_TIME]
            )
        )

    if CONF_MIN_HUMIDIFYING_OFF_TIME in config:
        cg.add(
            var.set_humidifying_minimum_off_time_in_sec(
                config[CONF_MIN_HUMIDIFYING_OFF_TIME]
            )
        )

    if CONF_MIN_HUMIDIFYING_RUN_TIME in config:
        cg.add(
            var.set_humidifying_minimum_run_time_in_sec(
                config[CONF_MIN_HUMIDIFYING_RUN_TIME]
            )
        )

    cg.add(var.set_idle_minimum_time_in_sec(config[CONF_MIN_IDLE_TIME]))

    cg.add(var.set_use_startup_delay(config[CONF_STARTUP_DELAY]))
    cg.add(var.set_normal_config(normal_config))

    await automation.build_automation(
        var.get_idle_action_trigger(), [], config[CONF_IDLE_ACTION]
    )

    if humidify_dehumidify_mode_available is True:
        cg.add(var.set_supports_humidify_dehumidify(True))
    else:
        cg.add(var.set_supports_humidify_dehumidify(False))

    if CONF_DEHUMIDIFY_ACTION in config:
        await automation.build_automation(
            var.get_dehumidify_action_trigger(), [], config[CONF_DEHUMIDIFY_ACTION]
        )
        cg.add(var.set_supports_dehumidify(True))
    if CONF_HUMIDIFY_ACTION in config:
        await automation.build_automation(
            var.get_humidify_action_trigger(), [], config[CONF_HUMIDIFY_ACTION]
        )
        cg.add(var.set_supports_humidify(True))
    if CONF_AUTO_MODE in config:
        await automation.build_automation(
            var.get_auto_mode_trigger(), [], config[CONF_AUTO_MODE]
        )
    if CONF_DEHUMIDIFY_MODE in config:
        await automation.build_automation(
            var.get_dehumidify_mode_trigger(), [], config[CONF_DEHUMIDIFY_MODE]
        )
        cg.add(var.set_supports_dehumidify(True))
    if CONF_DEHUMIDIFY_MODE in config:
        await automation.build_automation(
            var.get_dehumidify_mode_trigger(), [], config[CONF_DEHUMIDIFY_MODE]
        )
        cg.add(var.set_supports_dehumidify(True))
    if CONF_HUMIDIFY_MODE in config:
        await automation.build_automation(
            var.get_humidify_mode_trigger(), [], config[CONF_HUMIDIFY_MODE]
        )
        cg.add(var.set_supports_humidify(True))
    if CONF_OFF_MODE in config:
        await automation.build_automation(
            var.get_off_mode_trigger(), [], config[CONF_OFF_MODE]
        )
    if CONF_TARGET_HUMIDITY_CHANGE_ACTION in config:
        await automation.build_automation(
            var.get_humidity_change_trigger(),
            [],
            config[CONF_TARGET_HUMIDITY_CHANGE_ACTION],
        )

    if CONF_AWAY_CONFIG in config:
        away = config[CONF_AWAY_CONFIG]

        if humidify_dehumidify_mode_available is True:
            away_config = HygrostatHumidifierTargetTempConfig(
                away[CONF_DEFAULT_TARGET_HUMIDITY_LOW],
                away[CONF_DEFAULT_TARGET_HUMIDITY_HIGH],
            )
        elif CONF_DEFAULT_TARGET_HUMIDITY_HIGH in away:
            away_config = HygrostatHumidifierTargetTempConfig(
                away[CONF_DEFAULT_TARGET_HUMIDITY_HIGH]
            )
        elif CONF_DEFAULT_TARGET_HUMIDITY_LOW in away:
            away_config = HygrostatHumidifierTargetTempConfig(
                away[CONF_DEFAULT_TARGET_HUMIDITY_LOW]
            )
        cg.add(var.set_away_config(away_config))
