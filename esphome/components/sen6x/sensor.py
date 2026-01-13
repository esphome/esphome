from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALGORITHM_TUNING,
    CONF_ALTITUDE,
    CONF_ALTITUDE_COMPENSATION,
    CONF_AMBIENT_PRESSURE_COMPENSATION,
    CONF_CO2,
    CONF_FORMALDEHYDE,
    CONF_GAIN_FACTOR,
    CONF_GATING_MAX_DURATION_MINUTES,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_INDEX_OFFSET,
    CONF_LEARNING_TIME_GAIN_HOURS,
    CONF_LEARNING_TIME_OFFSET_HOURS,
    CONF_NORMALIZED_OFFSET_SLOPE,
    CONF_NOX,
    CONF_OFFSET,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_PM_10_0,
    CONF_PRESSURE,
    CONF_STD_INITIAL,
    CONF_STORE_BASELINE,
    CONF_TEMPERATURE,
    CONF_TEMPERATURE_COMPENSATION,
    CONF_TIME_CONSTANT,
    CONF_VOC,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_GAS,
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
    UNIT_PARTS_PER_BILLION,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)

CODEOWNERS = ["@martgras", "@mebner86"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

sen6x_ns = cg.esphome_ns.namespace("sen6x")
SEN6XComponent = sen6x_ns.class_(
    "SEN6XComponent", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)

CONF_SLOT = "slot"

# Actions
StartFanAction = sen6x_ns.class_("StartFanAction", automation.Action)


def _gas_sensor(
    *,
    index_offset: int,
    learning_time_offset: int,
    learning_time_gain: int,
    gating_max_duration: int,
    std_initial: int,
    gain_factor: int,
) -> cv.Schema:
    return sensor.sensor_schema(
        icon=ICON_RADIATOR,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_AQI,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(
        {
            cv.Optional(CONF_ALGORITHM_TUNING): cv.Schema(
                {
                    cv.Optional(CONF_INDEX_OFFSET, default=index_offset): cv.int_range(
                        1, 250
                    ),
                    cv.Optional(
                        CONF_LEARNING_TIME_OFFSET_HOURS, default=learning_time_offset
                    ): cv.int_range(1, 1000),
                    cv.Optional(
                        CONF_LEARNING_TIME_GAIN_HOURS, default=learning_time_gain
                    ): cv.int_range(1, 1000),
                    cv.Optional(
                        CONF_GATING_MAX_DURATION_MINUTES, default=gating_max_duration
                    ): cv.int_range(0, 3000),
                    cv.Optional(CONF_STD_INITIAL, default=std_initial): cv.int_range(
                        10, 5000
                    ),
                    cv.Optional(CONF_GAIN_FACTOR, default=gain_factor): cv.int_range(
                        1, 1000
                    ),
                }
            )
        }
    )


def float_previously_pct(value):
    if isinstance(value, str) and "%" in value:
        raise cv.Invalid(
            f"The value '{value}' is a percentage. Suggested value: {float(value.strip('%')) / 100}"
        )
    return value


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SEN6XComponent),
            cv.Required("type"): cv.one_of(
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
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION): cv.Schema(
                        {
                            cv.Optional(CONF_PRESSURE, default=1013): cv.int_range(
                                700, 1200
                            ),
                        }
                    ),
                    cv.Optional(CONF_ALTITUDE_COMPENSATION): cv.Schema(
                        {
                            cv.Optional(CONF_ALTITUDE, default=0): cv.int_range(
                                0, 3000
                            ),
                        }
                    ),
                }
            ),
            cv.Optional(CONF_FORMALDEHYDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_GAS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOC): _gas_sensor(
                index_offset=100,
                learning_time_offset=12,
                learning_time_gain=12,
                gating_max_duration=180,
                std_initial=50,
                gain_factor=230,
            ),
            cv.Optional(CONF_NOX): _gas_sensor(
                index_offset=1,
                learning_time_offset=12,
                learning_time_gain=12,
                gating_max_duration=720,
                std_initial=50,
                gain_factor=230,
            ),
            cv.Optional(CONF_STORE_BASELINE, default=True): cv.boolean,
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
            cv.Optional(CONF_TEMPERATURE_COMPENSATION): cv.Schema(
                {
                    cv.Optional(CONF_OFFSET, default=0): cv.float_,
                    cv.Optional(CONF_NORMALIZED_OFFSET_SLOPE, default=0): cv.All(
                        float_previously_pct, cv.float_
                    ),
                    cv.Optional(CONF_TIME_CONSTANT, default=0): cv.int_,
                    cv.Optional(CONF_SLOT, default=0): cv.int_range(0, 4),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6B))
)

SENSOR_MAP = {
    CONF_PM_1_0: "set_pm_1_0_sensor",
    CONF_PM_2_5: "set_pm_2_5_sensor",
    CONF_PM_4_0: "set_pm_4_0_sensor",
    CONF_PM_10_0: "set_pm_10_0_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_HUMIDITY: "set_humidity_sensor",
    CONF_VOC: "set_voc_sensor",
    CONF_NOX: "set_nox_sensor",
    CONF_CO2: "set_co2_sensor",
    CONF_FORMALDEHYDE: "set_hcho_sensor",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_type(config["type"]))

    for key, funcName in SENSOR_MAP.items():
        if cfg := config.get(key):
            sens = await sensor.new_sensor(cfg)
            cg.add(getattr(var, funcName)(sens))

    if cfg := config.get(CONF_VOC, {}).get(CONF_ALGORITHM_TUNING):
        cg.add(
            var.set_voc_algorithm_tuning(
                cfg[CONF_INDEX_OFFSET],
                cfg[CONF_LEARNING_TIME_OFFSET_HOURS],
                cfg[CONF_LEARNING_TIME_GAIN_HOURS],
                cfg[CONF_GATING_MAX_DURATION_MINUTES],
                cfg[CONF_STD_INITIAL],
                cfg[CONF_GAIN_FACTOR],
            )
        )
    if cfg := config.get(CONF_NOX, {}).get(CONF_ALGORITHM_TUNING):
        cg.add(
            var.set_nox_algorithm_tuning(
                cfg[CONF_INDEX_OFFSET],
                cfg[CONF_LEARNING_TIME_OFFSET_HOURS],
                cfg[CONF_LEARNING_TIME_GAIN_HOURS],
                cfg[CONF_GATING_MAX_DURATION_MINUTES],
                cfg[CONF_GAIN_FACTOR],
            )
        )
    if cfg := config.get(CONF_TEMPERATURE_COMPENSATION):
        cg.add(
            var.set_temperature_compensation(
                cfg[CONF_OFFSET],
                cfg[CONF_NORMALIZED_OFFSET_SLOPE],
                cfg[CONF_TIME_CONSTANT],
                cfg[CONF_SLOT],
            )
        )
    if cfg := config.get(CONF_CO2, {}).get(CONF_AMBIENT_PRESSURE_COMPENSATION):
        cg.add(
            var.set_pressure_compensation(
                cfg[CONF_PRESSURE],
            )
        )
    if cfg := config.get(CONF_CO2, {}).get(CONF_ALTITUDE_COMPENSATION):
        cg.add(
            var.set_altitude_compensation(
                cfg[CONF_ALTITUDE],
            )
        )


SEN6X_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(SEN6XComponent),
    }
)


@automation.register_action(
    "sen6x.start_fan_autoclean", StartFanAction, SEN6X_ACTION_SCHEMA
)
async def sen6x_fan_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
