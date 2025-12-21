from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
from esphome.components.aqi import AQI_CALCULATION_TYPE, CONF_AQI, CONF_CALCULATION_TYPE
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_PM_10_0,
    CONF_PM_SIZE,
    CONF_PMC_0_5,
    CONF_PMC_1_0,
    CONF_PMC_2_5,
    CONF_PMC_4_0,
    CONF_PMC_10_0,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    ICON_CHEMICAL_WEAPON,
    ICON_COUNTER,
    ICON_RULER,
    STATE_CLASS_MEASUREMENT,
    UNIT_COUNTS_PER_CUBIC_CENTIMETER,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_MICROMETER,
)

CODEOWNERS = ["@martgras"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["aqi", "sensirion_common"]

UNIT_INDEX = "index"

sps30_ns = cg.esphome_ns.namespace("sps30")
SPS30Component = sps30_ns.class_(
    "SPS30Component", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)

# Actions
StartFanAction = sps30_ns.class_("StartFanAction", automation.Action)
StartMeasurementAction = sps30_ns.class_("StartMeasurementAction", automation.Action)
StopMeasurementAction = sps30_ns.class_("StopMeasurementAction", automation.Action)

CONF_AUTO_CLEANING_INTERVAL = "auto_cleaning_interval"
CONF_IDLE_INTERVAL = "idle_interval"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPS30Component),
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_4_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PMC_0_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS_PER_CUBIC_CENTIMETER,
                icon=ICON_COUNTER,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PMC_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS_PER_CUBIC_CENTIMETER,
                icon=ICON_COUNTER,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PMC_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS_PER_CUBIC_CENTIMETER,
                icon=ICON_COUNTER,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PMC_4_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS_PER_CUBIC_CENTIMETER,
                icon=ICON_COUNTER,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PMC_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS_PER_CUBIC_CENTIMETER,
                icon=ICON_COUNTER,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_SIZE): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROMETER,
                icon=ICON_RULER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AQI): sensor.sensor_schema(
                unit_of_measurement=UNIT_INDEX,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Required(CONF_CALCULATION_TYPE): cv.enum(
                        AQI_CALCULATION_TYPE, upper=True
                    ),
                }
            ),
            cv.Optional(CONF_AUTO_CLEANING_INTERVAL): cv.update_interval,
            cv.Optional(CONF_IDLE_INTERVAL): cv.update_interval,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x69))
)


def validate_aqi_requires_pm(config):
    if CONF_AQI in config and (CONF_PM_2_5 not in config or CONF_PM_10_0 not in config):
        raise cv.Invalid(
            f"AQI sensor requires both '{CONF_PM_2_5}' and '{CONF_PM_10_0}' sensors to be configured"
        )
    return config


FINAL_VALIDATE_SCHEMA = validate_aqi_requires_pm


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_PM_1_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_1_0])
        cg.add(var.set_pm_1_0_sensor(sens))

    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))

    if CONF_PM_4_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_4_0])
        cg.add(var.set_pm_4_0_sensor(sens))

    if CONF_PM_10_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_10_0])
        cg.add(var.set_pm_10_0_sensor(sens))

    if CONF_PMC_0_5 in config:
        sens = await sensor.new_sensor(config[CONF_PMC_0_5])
        cg.add(var.set_pmc_0_5_sensor(sens))

    if CONF_PMC_1_0 in config:
        sens = await sensor.new_sensor(config[CONF_PMC_1_0])
        cg.add(var.set_pmc_1_0_sensor(sens))

    if CONF_PMC_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PMC_2_5])
        cg.add(var.set_pmc_2_5_sensor(sens))

    if CONF_PMC_4_0 in config:
        sens = await sensor.new_sensor(config[CONF_PMC_4_0])
        cg.add(var.set_pmc_4_0_sensor(sens))

    if CONF_PMC_10_0 in config:
        sens = await sensor.new_sensor(config[CONF_PMC_10_0])
        cg.add(var.set_pmc_10_0_sensor(sens))

    if CONF_PM_SIZE in config:
        sens = await sensor.new_sensor(config[CONF_PM_SIZE])
        cg.add(var.set_pm_size_sensor(sens))

    if CONF_AUTO_CLEANING_INTERVAL in config:
        cg.add(var.set_auto_cleaning_interval(config[CONF_AUTO_CLEANING_INTERVAL]))

    if CONF_IDLE_INTERVAL in config:
        cg.add(var.set_idle_interval(config[CONF_IDLE_INTERVAL]))

    if CONF_AQI in config:
        sens = await sensor.new_sensor(config[CONF_AQI])
        cg.add(var.set_aqi_sensor(sens))
        cg.add(var.set_aqi_calculation_type(config[CONF_AQI][CONF_CALCULATION_TYPE]))


SPS30_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(SPS30Component),
    }
)


@automation.register_action(
    "sps30.start_fan_autoclean", StartFanAction, SPS30_ACTION_SCHEMA
)
@automation.register_action(
    "sps30.start_measurement", StartMeasurementAction, SPS30_ACTION_SCHEMA
)
@automation.register_action(
    "sps30.stop_measurement", StopMeasurementAction, SPS30_ACTION_SCHEMA
)
async def sps30_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
