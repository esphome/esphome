import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMMAND,
    CONF_CUSTOM,
    CONF_DEST,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_SENSORS,
    CONF_SOURCE,
    CONF_TIME,
    CONF_VERSION,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_PERCENT,
    ICON_RADIATOR,
    ICON_THERMOMETER,
    ICON_TIMER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_HOUR,
    UNIT_MINUTE,
    UNIT_PERCENT,
    UNIT_WATT_HOURS,
)
from esphome.cpp_generator import MockObjClass
from esphome.types import ConfigType

from .. import (
    CONF_DELTASOL_BS2,
    CONF_DELTASOL_BS_2009,
    CONF_DELTASOL_BS_PLUS,
    CONF_DELTASOL_C,
    CONF_DELTASOL_CS2,
    CONF_DELTASOL_CS4,
    CONF_DELTASOL_CS_PLUS,
    CONF_VBUS_ID,
    VBus,
    vbus_ns,
)

DeltaSol_BS_Plus = vbus_ns.class_("DeltaSolBSPlusSensor", cg.Component)
DeltaSol_BS_2009 = vbus_ns.class_("DeltaSolBS2009Sensor", cg.Component)
DeltaSol_BS2 = vbus_ns.class_("DeltaSolBS2Sensor", cg.Component)
DeltaSol_C = vbus_ns.class_("DeltaSolCSensor", cg.Component)
DeltaSol_CS2 = vbus_ns.class_("DeltaSolCS2Sensor", cg.Component)
DeltaSol_CS4 = vbus_ns.class_("DeltaSolCS4Sensor", cg.Component)
DeltaSol_CS_Plus = vbus_ns.class_("DeltaSolCSPlusSensor", cg.Component)
VBusCustom = vbus_ns.class_("VBusCustomSensor", cg.Component)
VBusCustomSub = vbus_ns.class_("VBusCustomSubSensor", sensor.Sensor)

CONF_FLOW_RATE = "flow_rate"
CONF_HEAT_QUANTITY = "heat_quantity"
CONF_OPERATING_HOURS = "operating_hours"
CONF_OPERATING_HOURS_1 = "operating_hours_1"
CONF_OPERATING_HOURS_2 = "operating_hours_2"
CONF_PUMP_SPEED = "pump_speed"
CONF_PUMP_SPEED_1 = "pump_speed_1"
CONF_PUMP_SPEED_2 = "pump_speed_2"
CONF_TEMPERATURE_1 = "temperature_1"
CONF_TEMPERATURE_2 = "temperature_2"
CONF_TEMPERATURE_3 = "temperature_3"
CONF_TEMPERATURE_4 = "temperature_4"
CONF_TEMPERATURE_5 = "temperature_5"

TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    icon=ICON_THERMOMETER,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)
PUMP_SPEED_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    icon=ICON_PERCENT,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_EMPTY,
    state_class=STATE_CLASS_MEASUREMENT,
)
OPERATING_HOURS_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_HOUR,
    icon=ICON_TIMER,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_DURATION,
    state_class=STATE_CLASS_MEASUREMENT,
)
HEAT_QUANTITY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT_HOURS,
    icon=ICON_RADIATOR,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_TOTAL_INCREASING,
)
TIME_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_MINUTE,
    icon=ICON_TIMER,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_DURATION,
    state_class=STATE_CLASS_MEASUREMENT,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)
VERSION_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=2,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)
FLOW_RATE_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_EMPTY,
    state_class=STATE_CLASS_MEASUREMENT,
)

# Configuration key -> (schema of that value, C++ setter to pass it to)
SENSOR_FIELDS = {
    CONF_TEMPERATURE_1: (TEMPERATURE_SCHEMA, "set_temperature1_sensor"),
    CONF_TEMPERATURE_2: (TEMPERATURE_SCHEMA, "set_temperature2_sensor"),
    CONF_TEMPERATURE_3: (TEMPERATURE_SCHEMA, "set_temperature3_sensor"),
    CONF_TEMPERATURE_4: (TEMPERATURE_SCHEMA, "set_temperature4_sensor"),
    CONF_TEMPERATURE_5: (TEMPERATURE_SCHEMA, "set_temperature5_sensor"),
    CONF_PUMP_SPEED: (PUMP_SPEED_SCHEMA, "set_pump_speed_sensor"),
    CONF_PUMP_SPEED_1: (PUMP_SPEED_SCHEMA, "set_pump_speed1_sensor"),
    CONF_PUMP_SPEED_2: (PUMP_SPEED_SCHEMA, "set_pump_speed2_sensor"),
    CONF_OPERATING_HOURS: (OPERATING_HOURS_SCHEMA, "set_operating_hours_sensor"),
    CONF_OPERATING_HOURS_1: (OPERATING_HOURS_SCHEMA, "set_operating_hours1_sensor"),
    CONF_OPERATING_HOURS_2: (OPERATING_HOURS_SCHEMA, "set_operating_hours2_sensor"),
    CONF_HEAT_QUANTITY: (HEAT_QUANTITY_SCHEMA, "set_heat_quantity_sensor"),
    CONF_TIME: (TIME_SCHEMA, "set_time_sensor"),
    CONF_VERSION: (VERSION_SCHEMA, "set_version_sensor"),
    CONF_FLOW_RATE: (FLOW_RATE_SCHEMA, "set_flow_rate_sensor"),
}

TEMPERATURES_1_TO_4 = [
    CONF_TEMPERATURE_1,
    CONF_TEMPERATURE_2,
    CONF_TEMPERATURE_3,
    CONF_TEMPERATURE_4,
]
TWO_RELAYS = [
    CONF_PUMP_SPEED_1,
    CONF_PUMP_SPEED_2,
    CONF_OPERATING_HOURS_1,
    CONF_OPERATING_HOURS_2,
]

# Model -> (C++ class, VBus source address of that controller, supported fields)
MODELS = {
    CONF_DELTASOL_BS_PLUS: (
        DeltaSol_BS_Plus,
        0x4221,
        [
            *TEMPERATURES_1_TO_4,
            *TWO_RELAYS,
            CONF_HEAT_QUANTITY,
            CONF_TIME,
            CONF_VERSION,
        ],
    ),
    CONF_DELTASOL_BS_2009: (
        DeltaSol_BS_2009,
        0x427B,
        [
            *TEMPERATURES_1_TO_4,
            *TWO_RELAYS,
            CONF_HEAT_QUANTITY,
            CONF_TIME,
            CONF_VERSION,
        ],
    ),
    CONF_DELTASOL_BS2: (
        DeltaSol_BS2,
        0x4278,
        [*TEMPERATURES_1_TO_4, *TWO_RELAYS, CONF_HEAT_QUANTITY, CONF_VERSION],
    ),
    CONF_DELTASOL_C: (
        DeltaSol_C,
        0x4212,
        [*TEMPERATURES_1_TO_4, *TWO_RELAYS, CONF_HEAT_QUANTITY, CONF_TIME],
    ),
    CONF_DELTASOL_CS2: (
        DeltaSol_CS2,
        0x1121,
        [
            *TEMPERATURES_1_TO_4,
            CONF_PUMP_SPEED,
            CONF_OPERATING_HOURS,
            CONF_HEAT_QUANTITY,
            CONF_VERSION,
        ],
    ),
    CONF_DELTASOL_CS4: (
        DeltaSol_CS4,
        0x1122,
        [
            *TEMPERATURES_1_TO_4,
            CONF_TEMPERATURE_5,
            *TWO_RELAYS,
            CONF_HEAT_QUANTITY,
            CONF_TIME,
            CONF_VERSION,
            CONF_FLOW_RATE,
        ],
    ),
    CONF_DELTASOL_CS_PLUS: (
        DeltaSol_CS_Plus,
        0x2211,
        [
            *TEMPERATURES_1_TO_4,
            CONF_TEMPERATURE_5,
            *TWO_RELAYS,
            CONF_HEAT_QUANTITY,
            CONF_TIME,
            CONF_VERSION,
            CONF_FLOW_RATE,
        ],
    ),
}


def _model_schema(model_class: MockObjClass, fields: list[str]) -> cv.Schema:
    return cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(model_class),
            cv.GenerateID(CONF_VBUS_ID): cv.use_id(VBus),
            **{cv.Optional(field): SENSOR_FIELDS[field][0] for field in fields},
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        **{
            model: _model_schema(model_class, fields)
            for model, (model_class, _, fields) in MODELS.items()
        },
        CONF_CUSTOM: cv.COMPONENT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(VBusCustom),
                cv.GenerateID(CONF_VBUS_ID): cv.use_id(VBus),
                cv.Optional(CONF_COMMAND): cv.uint16_t,
                cv.Optional(CONF_SOURCE): cv.uint16_t,
                cv.Optional(CONF_DEST): cv.uint16_t,
                cv.Required(CONF_SENSORS): cv.ensure_list(
                    sensor.sensor_schema().extend(
                        {
                            cv.GenerateID(): cv.declare_id(VBusCustomSub),
                            cv.Required(CONF_LAMBDA): cv.lambda_,
                        }
                    )
                ),
            }
        ),
    },
    key=CONF_MODEL,
    lower=True,
    space="_",
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if config[CONF_MODEL] == CONF_CUSTOM:
        for key in (CONF_COMMAND, CONF_SOURCE, CONF_DEST):
            if (value := config.get(key)) is not None:
                cg.add(getattr(var, f"set_{key}")(value))
        sensors = []
        for conf in config[CONF_SENSORS]:
            sens = await sensor.new_sensor(conf)
            lambda_ = await cg.process_lambda(
                conf[CONF_LAMBDA],
                [(cg.std_vector.template(cg.uint8), "x")],
                return_type=cg.float_,
            )
            cg.add(sens.set_message_parser(lambda_))
            sensors.append(sens)
        cg.add(var.set_sensors(sensors))
    else:
        _, source, fields = MODELS[config[CONF_MODEL]]
        cg.add(var.set_command(0x0100))
        cg.add(var.set_source(source))
        cg.add(var.set_dest(0x0010))
        sub_sensors = sensor.sub_sensors(config)
        for field in fields:
            await sub_sensors(field, getattr(var, SENSOR_FIELDS[field][1]))

    vbus = await cg.get_variable(config[CONF_VBUS_ID])
    cg.add(vbus.register_listener(var))
