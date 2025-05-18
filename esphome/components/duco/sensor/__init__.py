import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CO2,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
    UNIT_SECOND,
)

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

UNIT_DAYS = "days"

CONF_FILTER_REMAINING = "filter_remaining"
CONF_FLOW_LEVEL = "flow_level"
CONF_TIME_REMAINING = "time_remaining"
CONF_BYPASS = "bypass"
CONF_TEMPERATURE_ODA = "temperature_oda"
CONF_TEMPERATURE_SUP = "temperature_sup"
CONF_TEMPERATURE_ETA = "temperature_eta"
CONF_TEMPERATURE_EHA = "temperature_eha"

SENSOR_TYPE_ODA = 0
SENSOR_TYPE_SUP = 1
SENSOR_TYPE_ETA = 2
SENSOR_TYPE_EHA = 3


duco_ns = cg.esphome_ns.namespace("duco")
DucoCo2Sensor = duco_ns.class_("DucoCo2Sensor", cg.PollingComponent, sensor.Sensor)
DucoHumiditySensor = duco_ns.class_(
    "DucoHumiditySensor", cg.PollingComponent, sensor.Sensor
)
DucoTemperatureSensor = duco_ns.class_(
    "DucoTemperatureSensor", cg.PollingComponent, sensor.Sensor
)
DucoBoxTemperatureSensor = duco_ns.class_(
    "DucoBoxTemperatureSensor", cg.PollingComponent, sensor.Sensor
)
DucoBypassSensor = duco_ns.class_(
    "DucoBypassSensor", cg.PollingComponent, sensor.Sensor
)
DucoFilterRemainingSensor = duco_ns.class_(
    "DucoFilterRemainingSensor", cg.PollingComponent, sensor.Sensor
)
DucoFlowLevelSensor = duco_ns.class_(
    "DucoFlowLevelSensor", cg.PollingComponent, sensor.Sensor
)
DucoStateTimeRemainingSensor = duco_ns.class_(
    "DucoStateTimeRemainingSensor", cg.PollingComponent, sensor.Sensor
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
        cv.Optional(CONF_TEMPERATURE): cv.ensure_list(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            .extend(
                {
                    cv.GenerateID(): cv.declare_id(DucoTemperatureSensor),
                    cv.Required(CONF_ADDRESS): cv.int_range(0, 68),
                }
            )
            .extend(cv.polling_component_schema("60s"))
        ),
        cv.Optional(CONF_HUMIDITY): cv.ensure_list(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            .extend(
                {
                    cv.GenerateID(): cv.declare_id(DucoHumiditySensor),
                    cv.Required(CONF_ADDRESS): cv.int_range(0, 68),
                }
            )
            .extend(cv.polling_component_schema("60s"))
        ),
        cv.Optional(CONF_FILTER_REMAINING): sensor.sensor_schema(
            unit_of_measurement=UNIT_DAYS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend({cv.GenerateID(): cv.declare_id(DucoFilterRemainingSensor)})
        .extend(cv.polling_component_schema("1200s")),
        cv.Optional(CONF_FLOW_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend({cv.GenerateID(): cv.declare_id(DucoFlowLevelSensor)})
        .extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_TIME_REMAINING): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend({cv.GenerateID(): cv.declare_id(DucoStateTimeRemainingSensor)})
        .extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_TEMPERATURE_ODA): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend({cv.GenerateID(): cv.declare_id(DucoBoxTemperatureSensor)})
        .extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_TEMPERATURE_SUP): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend({cv.GenerateID(): cv.declare_id(DucoBoxTemperatureSensor)})
        .extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_TEMPERATURE_ETA): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend({cv.GenerateID(): cv.declare_id(DucoBoxTemperatureSensor)})
        .extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_TEMPERATURE_EHA): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend({cv.GenerateID(): cv.declare_id(DucoBoxTemperatureSensor)})
        .extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_BYPASS): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend({cv.GenerateID(): cv.declare_id(DucoBypassSensor)})
        .extend(cv.polling_component_schema("60s")),
    }
).extend(DUCO_COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DUCO_ID])

    if CONF_CO2 in config:
        for co2_sensor_config in config[CONF_CO2]:
            sensvar = cg.new_Pvariable(co2_sensor_config[CONF_ID])
            await cg.register_component(sensvar, co2_sensor_config)
            await sensor.register_sensor(sensvar, co2_sensor_config)
            cg.add(sensvar.set_parent(parent))
            cg.add(sensvar.set_address(co2_sensor_config[CONF_ADDRESS]))

    if CONF_HUMIDITY in config:
        for humidity_sensor_config in config[CONF_HUMIDITY]:
            sensvar = cg.new_Pvariable(humidity_sensor_config[CONF_ID])
            await cg.register_component(sensvar, humidity_sensor_config)
            await sensor.register_sensor(sensvar, humidity_sensor_config)
            cg.add(sensvar.set_parent(parent))
            cg.add(sensvar.set_address(humidity_sensor_config[CONF_ADDRESS]))

    if CONF_TEMPERATURE in config:
        for temperature_sensor_config in config[CONF_TEMPERATURE]:
            sensvar = cg.new_Pvariable(temperature_sensor_config[CONF_ID])
            await cg.register_component(sensvar, temperature_sensor_config)
            await sensor.register_sensor(sensvar, temperature_sensor_config)
            cg.add(sensvar.set_parent(parent))
            cg.add(sensvar.set_address(temperature_sensor_config[CONF_ADDRESS]))

    if CONF_FILTER_REMAINING in config:
        filter_remaining_config = config[CONF_FILTER_REMAINING]
        sensvar = cg.new_Pvariable(filter_remaining_config[CONF_ID])
        await cg.register_component(sensvar, filter_remaining_config)
        await sensor.register_sensor(sensvar, filter_remaining_config)
        cg.add(sensvar.set_parent(parent))

    if CONF_FLOW_LEVEL in config:
        flow_level_config = config[CONF_FLOW_LEVEL]
        sensvar = cg.new_Pvariable(flow_level_config[CONF_ID])
        await cg.register_component(sensvar, flow_level_config)
        await sensor.register_sensor(sensvar, flow_level_config)
        cg.add(sensvar.set_parent(parent))

    if CONF_TIME_REMAINING in config:
        time_remaining_config = config[CONF_TIME_REMAINING]
        sensvar = cg.new_Pvariable(time_remaining_config[CONF_ID])
        await cg.register_component(sensvar, time_remaining_config)
        await sensor.register_sensor(sensvar, time_remaining_config)
        cg.add(sensvar.set_parent(parent))

    if CONF_TEMPERATURE_ODA in config:
        oda_temperature_config = config[CONF_TEMPERATURE_ODA]
        sensvar = cg.new_Pvariable(oda_temperature_config[CONF_ID])
        await cg.register_component(sensvar, oda_temperature_config)
        await sensor.register_sensor(sensvar, oda_temperature_config)
        cg.add(sensvar.set_parent(parent))
        cg.add(sensvar.set_type(SENSOR_TYPE_ODA))

    if CONF_TEMPERATURE_SUP in config:
        sup_temperature_config = config[CONF_TEMPERATURE_SUP]
        sensvar = cg.new_Pvariable(sup_temperature_config[CONF_ID])
        await cg.register_component(sensvar, sup_temperature_config)
        await sensor.register_sensor(sensvar, sup_temperature_config)
        cg.add(sensvar.set_parent(parent))
        cg.add(sensvar.set_type(SENSOR_TYPE_SUP))

    if CONF_TEMPERATURE_ETA in config:
        eta_temperature_config = config[CONF_TEMPERATURE_ETA]
        sensvar = cg.new_Pvariable(eta_temperature_config[CONF_ID])
        await cg.register_component(sensvar, eta_temperature_config)
        await sensor.register_sensor(sensvar, eta_temperature_config)
        cg.add(sensvar.set_parent(parent))
        cg.add(sensvar.set_type(SENSOR_TYPE_ETA))

    if CONF_TEMPERATURE_EHA in config:
        eha_temperature_config = config[CONF_TEMPERATURE_EHA]
        sensvar = cg.new_Pvariable(eha_temperature_config[CONF_ID])
        await cg.register_component(sensvar, eha_temperature_config)
        await sensor.register_sensor(sensvar, eha_temperature_config)
        cg.add(sensvar.set_parent(parent))
        cg.add(sensvar.set_type(SENSOR_TYPE_EHA))

    if CONF_BYPASS in config:
        bypass_config = config[CONF_BYPASS]
        sensvar = cg.new_Pvariable(bypass_config[CONF_ID])
        await cg.register_component(sensvar, bypass_config)
        await sensor.register_sensor(sensvar, bypass_config)
        cg.add(sensvar.set_parent(parent))
