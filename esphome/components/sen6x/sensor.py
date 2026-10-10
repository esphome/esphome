import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
from esphome.components.const import CONF_NOX_INDEX, CONF_VOC_INDEX
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALGORITHM_TUNING,
    CONF_CO2,
    CONF_FORMALDEHYDE,
    CONF_GAIN_FACTOR,
    CONF_GATING_MAX_DURATION_MINUTES,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_INDEX_OFFSET,
    CONF_LEARNING_TIME_GAIN_HOURS,
    CONF_LEARNING_TIME_OFFSET_HOURS,
    CONF_NOX,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_PM_10_0,
    CONF_STD_INITIAL,
    CONF_TEMPERATURE,
    CONF_TYPE,
    CONF_VOC,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_TEMPERATURE,
    ICON_CHEMICAL_WEAPON,
    ICON_MOLECULE_CO2,
    ICON_RADIATOR,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

CODEOWNERS = ["@martgras", "@mebner86", "@tuct"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

sen6x_ns = cg.esphome_ns.namespace("sen6x")
SEN6XComponent = sen6x_ns.class_(
    "SEN6XComponent", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)


def _gas_index_schema(
    *,
    index_offset: int,
    gating_max_duration: int,
    std_initial: int | None,
) -> cv.Schema:
    """Sensor schema for a gas index sensor with optional algorithm tuning.

    std_initial is only configurable for VOC; the NOx algorithm requires 50.
    """
    tuning_schema = {
        cv.Optional(CONF_INDEX_OFFSET, default=index_offset): cv.int_range(
            min=1, max=250
        ),
        cv.Optional(CONF_LEARNING_TIME_OFFSET_HOURS, default=12): cv.int_range(
            min=1, max=1000
        ),
        cv.Optional(CONF_LEARNING_TIME_GAIN_HOURS, default=12): cv.int_range(
            min=1, max=1000
        ),
        cv.Optional(
            CONF_GATING_MAX_DURATION_MINUTES, default=gating_max_duration
        ): cv.int_range(min=0, max=3000),
        cv.Optional(CONF_GAIN_FACTOR, default=230): cv.int_range(min=1, max=1000),
    }
    if std_initial is not None:
        tuning_schema[cv.Optional(CONF_STD_INITIAL, default=std_initial)] = (
            cv.int_range(min=10, max=5000)
        )
    return sensor.sensor_schema(
        icon=ICON_RADIATOR,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend({cv.Optional(CONF_ALGORITHM_TUNING): cv.Schema(tuning_schema)})


CONFIG_SCHEMA = cv.All(
    cv.rename_key(CONF_VOC, CONF_VOC_INDEX, removed_in="2027.2.0", component="sen6x"),
    cv.rename_key(CONF_NOX, CONF_NOX_INDEX, removed_in="2027.2.0", component="sen6x"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SEN6XComponent),
            cv.Optional(CONF_TYPE): cv.one_of(
                "SEN62", "SEN63C", "SEN65", "SEN66", "SEN68", "SEN69C", upper=True
            ),
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
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOC_INDEX): _gas_index_schema(
                index_offset=100,
                gating_max_duration=180,
                std_initial=50,
            ),
            cv.Optional(CONF_NOX_INDEX): _gas_index_schema(
                index_offset=1,
                gating_max_duration=720,
                std_initial=None,
            ),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FORMALDEHYDE): sensor.sensor_schema(
                unit_of_measurement="ppb",
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6B)),
)

SENSOR_MAP = {
    CONF_PM_1_0: "set_pm_1_0_sensor",
    CONF_PM_2_5: "set_pm_2_5_sensor",
    CONF_PM_4_0: "set_pm_4_0_sensor",
    CONF_PM_10_0: "set_pm_10_0_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_HUMIDITY: "set_humidity_sensor",
    CONF_VOC_INDEX: "set_voc_sensor",
    CONF_NOX_INDEX: "set_nox_sensor",
    CONF_CO2: "set_co2_sensor",
    CONF_FORMALDEHYDE: "set_hcho_sensor",
}


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_TYPE in config:
        cg.add(var.set_type(config[CONF_TYPE]))

    for key, func_name in SENSOR_MAP.items():
        if cfg := config.get(key):
            sens = await sensor.new_sensor(cfg)
            cg.add(getattr(var, func_name)(sens))

    for key, setter in (
        (CONF_VOC_INDEX, "set_voc_algorithm_tuning"),
        (CONF_NOX_INDEX, "set_nox_algorithm_tuning"),
    ):
        if (tuning := config.get(key, {}).get(CONF_ALGORITHM_TUNING)) is not None:
            args = [
                tuning[CONF_INDEX_OFFSET],
                tuning[CONF_LEARNING_TIME_OFFSET_HOURS],
                tuning[CONF_LEARNING_TIME_GAIN_HOURS],
                tuning[CONF_GATING_MAX_DURATION_MINUTES],
            ]
            # std_initial is in the schema for VOC only
            if (std_initial := tuning.get(CONF_STD_INITIAL)) is not None:
                args.append(std_initial)
            args.append(tuning[CONF_GAIN_FACTOR])
            cg.add(getattr(var, setter)(*args))
