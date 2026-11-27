import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_ICON,
    CONF_ID,
    CONF_SENSOR,
    CONF_UNIT_OF_MEASUREMENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_KILOVOLT_AMPS,
    UNIT_KILOVOLT_AMPS_REACTIVE,
    UNIT_KILOWATT,
    UNIT_MILLIAMP,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
)
from esphome.core.entity_helpers import inherit_property_from
from esphome.types import ConfigType

derivative_ns = cg.esphome_ns.namespace("derivative")
DerivativeSensor = derivative_ns.class_("DerivativeSensor", sensor.Sensor, cg.Component)

# Milliseconds in each time unit.
DERIVATIVE_TIME_UNITS = {
    "ms": 1,
    "s": 1000,
    "min": 60000,
    "h": 3600000,
    "d": 86400000,
}

CONF_TIME_UNIT = "time_unit"


# Rates that integration turns into a total by appending the time unit (W + h = Wh).
RATE_UNITS = frozenset(
    {
        UNIT_AMPERE,
        UNIT_KILOVOLT_AMPS,
        UNIT_KILOVOLT_AMPS_REACTIVE,
        UNIT_KILOWATT,
        UNIT_MILLIAMP,
        UNIT_VOLT_AMPS,
        UNIT_VOLT_AMPS_REACTIVE,
        UNIT_WATT,
    }
)


def inherit_unit_of_measurement(uom: str, config: ConfigType) -> str:
    # The reverse of integration: kWh with time_unit "h" becomes kW, m with "s" becomes m/s.
    suffix = config[CONF_TIME_UNIT]
    if (prefix := uom.removesuffix(suffix)) in RATE_UNITS and prefix != uom:
        return prefix
    return f"{uom}/{suffix}"


CONFIG_SCHEMA = (
    sensor.sensor_schema(DerivativeSensor, state_class=STATE_CLASS_MEASUREMENT)
    .extend(
        {
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_TIME_UNIT, default="s"): cv.one_of(
                *DERIVATIVE_TIME_UNITS, lower=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


FINAL_VALIDATE_SCHEMA = cv.All(
    inherit_property_from(CONF_ICON, CONF_SENSOR),
    inherit_property_from(
        CONF_UNIT_OF_MEASUREMENT, CONF_SENSOR, transform=inherit_unit_of_measurement
    ),
    inherit_property_from(CONF_ACCURACY_DECIMALS, CONF_SENSOR),
)


async def to_code(config: ConfigType) -> None:
    source = await cg.get_variable(config[CONF_SENSOR])
    var = cg.new_Pvariable(
        config[CONF_ID],
        cg.TemplateArguments(DERIVATIVE_TIME_UNITS[config[CONF_TIME_UNIT]]),
        source,
    )
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
