from esphome import automation
import esphome.codegen as cg
from esphome.components import microphone, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MEASUREMENT_DURATION,
    CONF_MICROPHONE,
    DEVICE_CLASS_SOUND_PRESSURE,
    PLATFORM_ESP32,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL,
)

AUTOLOAD = ["audio"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["microphone"]


CONF_PASSIVE = "passive"
CONF_PEAK = "peak"
CONF_RMS = "rms"

sound_level_ns = cg.esphome_ns.namespace("sound_level")
SoundLevelComponent = sound_level_ns.class_("SoundLevelComponent", cg.Component)

StartAction = sound_level_ns.class_("StartAction", automation.Action)
StopAction = sound_level_ns.class_("StopAction", automation.Action)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SoundLevelComponent),
            cv.Optional(CONF_MEASUREMENT_DURATION, default="1000ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=50),
                    max=cv.TimePeriod(seconds=60),
                ),
            ),
            cv.Optional(
                CONF_MICROPHONE, default={}
            ): microphone.microphone_source_schema(
                min_bits_per_sample=16,
                max_bits_per_sample=16,
            ),
            cv.Required(CONF_PASSIVE): cv.boolean,
            cv.Optional(CONF_PEAK): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_SOUND_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RMS): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_SOUND_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32]),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mic_source = await microphone.microphone_source_to_code(
        config[CONF_MICROPHONE], passive=config[CONF_PASSIVE]
    )
    cg.add(var.set_microphone_source(mic_source))

    cg.add(var.set_measurement_duration(config[CONF_MEASUREMENT_DURATION]))

    if peak_config := config.get(CONF_PEAK):
        sens = await sensor.new_sensor(peak_config)
        cg.add(var.set_peak_sensor(sens))
    if rms_config := config.get(CONF_RMS):
        sens = await sensor.new_sensor(rms_config)
        cg.add(var.set_rms_sensor(sens))


SOUND_LEVEL_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(SoundLevelComponent),
    }
)


@automation.register_action("sound_level.start", StartAction, SOUND_LEVEL_ACTION_SCHEMA)
@automation.register_action("sound_level.stop", StopAction, SOUND_LEVEL_ACTION_SCHEMA)
async def sound_level_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
