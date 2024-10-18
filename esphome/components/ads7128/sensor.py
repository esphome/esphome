import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_ID,
    CONF_OVERSAMPLING,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)

from . import ADS7128Component, ads7128_ns

DEPENDENCIES = ["ads7128"]

ADS7128Oversamping = ads7128_ns.enum("ADS7128Oversamping")
OVERSAMPLING = {
    "1": ADS7128Oversamping.ADS7128_OVERSAMPLING_1,
    "2": ADS7128Oversamping.ADS7128_OVERSAMPLING_2,
    "4": ADS7128Oversamping.ADS7128_OVERSAMPLING_4,
    "8": ADS7128Oversamping.ADS7128_OVERSAMPLING_8,
    "16": ADS7128Oversamping.ADS7128_OVERSAMPLING_16,
    "32": ADS7128Oversamping.ADS7128_OVERSAMPLING_32,
    "64": ADS7128Oversamping.ADS7128_OVERSAMPLING_64,
    # "128": ADS7128Oversamping.ADS7128_OVERSAMPLING_128,
}

ADS7128CycleTime = ads7128_ns.enum("ADS7128CycleTime")
CYCLE_TIME = {
    "1": ADS7128CycleTime.ADS7128_CYCLE_TIME_1,
    "1.5": ADS7128CycleTime.ADS7128_CYCLE_TIME_1P5,
    "2": ADS7128CycleTime.ADS7128_CYCLE_TIME_2,
    "3": ADS7128CycleTime.ADS7128_CYCLE_TIME_3,
    "4": ADS7128CycleTime.ADS7128_CYCLE_TIME_4,
    "6": ADS7128CycleTime.ADS7128_CYCLE_TIME_6,
    "8": ADS7128CycleTime.ADS7128_CYCLE_TIME_8,
    "12": ADS7128CycleTime.ADS7128_CYCLE_TIME_12,
    "16": ADS7128CycleTime.ADS7128_CYCLE_TIME_16,
    "24": ADS7128CycleTime.ADS7128_CYCLE_TIME_24,
    "32": ADS7128CycleTime.ADS7128_CYCLE_TIME_32,
    "48": ADS7128CycleTime.ADS7128_CYCLE_TIME_48,
    "64": ADS7128CycleTime.ADS7128_CYCLE_TIME_64,
    "96": ADS7128CycleTime.ADS7128_CYCLE_TIME_96,
    "128": ADS7128CycleTime.ADS7128_CYCLE_TIME_128,
    "192": ADS7128CycleTime.ADS7128_CYCLE_TIME_192,
    "256": ADS7128CycleTime.ADS7128_CYCLE_TIME_256,
    "384": ADS7128CycleTime.ADS7128_CYCLE_TIME_384,
    "512": ADS7128CycleTime.ADS7128_CYCLE_TIME_512,
    "768": ADS7128CycleTime.ADS7128_CYCLE_TIME_768,
    "1024": ADS7128CycleTime.ADS7128_CYCLE_TIME_1024,
    "1536": ADS7128CycleTime.ADS7128_CYCLE_TIME_1536,
    "2048": ADS7128CycleTime.ADS7128_CYCLE_TIME_2048,
    "3072": ADS7128CycleTime.ADS7128_CYCLE_TIME_3072,
    "4096": ADS7128CycleTime.ADS7128_CYCLE_TIME_4096,
    "6144": ADS7128CycleTime.ADS7128_CYCLE_TIME_6144,
}

ADS7128RmsSamples = ads7128_ns.enum("ADS7128RmsSamples")
RMS_SAMPLES = {
    "1024": ADS7128RmsSamples.ADS7128_RMS_SAMPLES_1024,
    "4096": ADS7128RmsSamples.ADS7128_RMS_SAMPLES_4096,
    "16384": ADS7128RmsSamples.ADS7128_RMS_SAMPLES_16384,
    "65536": ADS7128RmsSamples.ADS7128_RMS_SAMPLES_65536,
}

ADS7128Sensor = ads7128_ns.class_("ADS7128Sensor", sensor.Sensor, cg.PollingComponent)

CONF_ADS7128_ID = "ads7128_id"
CONF_RMS = "rms"
CONF_CYCLE_TIME = "cycle_time"
CONF_RMS_SAMPLES = "rms_samples"
CONFIG_SCHEMA = (
    sensor.sensor_schema(
        ADS7128Sensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_ADS7128_ID): cv.use_id(ADS7128Component),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
            cv.Optional(CONF_OVERSAMPLING, default=1): cv.enum(
                OVERSAMPLING, upper=True, space="_"
            ),
            cv.Optional(CONF_CYCLE_TIME, default=1): cv.enum(
                CYCLE_TIME, upper=True, space="_"
            ),
            cv.Optional(CONF_RMS, default=False): cv.boolean,
            cv.Optional(CONF_RMS_SAMPLES, default=1024): cv.enum(
                RMS_SAMPLES, upper=True, space="_"
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await sensor.register_sensor(var, config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_ADS7128_ID])

    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_oversampling(config[CONF_OVERSAMPLING]))
    cg.add(var.set_cycle_time(config[CONF_CYCLE_TIME]))
    cg.add(var.set_rms(config[CONF_RMS]))
    cg.add(var.set_rms_samples(config[CONF_RMS_SAMPLES]))
