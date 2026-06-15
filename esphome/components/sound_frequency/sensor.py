from esphome import automation
import esphome.codegen as cg
from esphome.components import microphone, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_ID,
    CONF_MEASUREMENT_DURATION,
    CONF_MICROPHONE,
    CONF_SAMPLE_RATE,
    DEVICE_CLASS_FREQUENCY,
    PLATFORM_ESP32,
    STATE_CLASS_MEASUREMENT,
    UNIT_HERTZ,
)

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@ljaffy"]
DEPENDENCIES = ["microphone"]


CONF_PASSIVE = "passive"

sound_frequency_ns = cg.esphome_ns.namespace("sound_frequency")
SoundFrequencyComponent = sound_frequency_ns.class_(
    "SoundFrequencyComponent", cg.Component
)

StartAction = sound_frequency_ns.class_("StartAction", automation.Action)
StopAction = sound_frequency_ns.class_("StopAction", automation.Action)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SoundFrequencyComponent),
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
            cv.Optional(CONF_SAMPLE_RATE): cv.positive_int,
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_FREQUENCY,
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

    if sample_rate := config.get(CONF_SAMPLE_RATE):
        cg.add(var.set_sample_rate(sample_rate))

    if freq_config := config.get(CONF_FREQUENCY):
        sens = await sensor.new_sensor(freq_config)
        cg.add(var.set_frequency_sensor(sens))


SOUND_FREQUENCY_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(SoundFrequencyComponent),
    }
)


@automation.register_action(
    "sound_frequency.start",
    StartAction,
    SOUND_FREQUENCY_ACTION_SCHEMA,
    synchronous=True,
)
async def sound_frequency_start_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "sound_frequency.stop", StopAction, SOUND_FREQUENCY_ACTION_SCHEMA, synchronous=True
)
async def sound_frequency_stop_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
