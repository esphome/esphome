import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CO2,
    CONF_ID,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

UNIT_DAYS = "days"

CONF_FILTER_REMAINING = "filter_remaining"

duco_ns = cg.esphome_ns.namespace("duco")
DucoCo2Sensor = duco_ns.class_("DucoCo2Sensor", cg.PollingComponent, sensor.Sensor)
DucoFilterRemainingSensor = duco_ns.class_(
    "DucoFilterRemainingSensor", cg.PollingComponent, sensor.Sensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CO2): cv.ensure_list(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            .extend(
                {
                    cv.GenerateID(): cv.declare_id(DucoCo2Sensor),
                    cv.Required(CONF_ADDRESS): cv.int_range(0, 68),
                }
            )
            .extend(cv.polling_component_schema("60s"))
        ),
        cv.Optional(CONF_FILTER_REMAINING): sensor.sensor_schema(
            unit_of_measurement=UNIT_DAYS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend({cv.GenerateID(): cv.declare_id(DucoFilterRemainingSensor)})
        .extend(cv.polling_component_schema("1200s")),
    }
).extend(DUCO_COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DUCO_ID])
    for co2_sensor_config in config[CONF_CO2]:
        sensvar = cg.new_Pvariable(co2_sensor_config[CONF_ID])
        await cg.register_component(sensvar, co2_sensor_config)
        await sensor.register_sensor(sensvar, co2_sensor_config)
        cg.add(sensvar.set_parent(parent))
        cg.add(sensvar.set_address(co2_sensor_config[CONF_ADDRESS]))

    if CONF_FILTER_REMAINING in config:
        filter_remaining_config = config[CONF_FILTER_REMAINING]
        sensvar = cg.new_Pvariable(filter_remaining_config[CONF_ID])
        await cg.register_component(sensvar, filter_remaining_config)
        await sensor.register_sensor(sensvar, filter_remaining_config)
        cg.add(sensvar.set_parent(parent))
