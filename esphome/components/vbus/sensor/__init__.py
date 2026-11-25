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
    DEVICE_CLASS_EMPTY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_PERCENT,
    ICON_RADIATOR,
    ICON_THERMOMETER,
    ICON_TIMER,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)
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

DeltaSol_BS_Plus = vbus_ns.class_("DeltaSolBSPlusSensor")
DeltaSol_BS_2009 = vbus_ns.class_("DeltaSolBS2009Sensor")
DeltaSol_BS2 = vbus_ns.class_("DeltaSolBS2Sensor")
DeltaSol_C = vbus_ns.class_("DeltaSolCSensor")
DeltaSol_CS2 = vbus_ns.class_("DeltaSolCS2Sensor")
DeltaSol_CS4 = vbus_ns.class_("DeltaSolCS4Sensor")
DeltaSol_CS_Plus = vbus_ns.class_("DeltaSolCSPlusSensor")
VBusCustom = vbus_ns.class_("VBusCustomSensor")
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

_TEMPERATURE_SCHEMA = sensor.sensor_schema(
    kind=sensor.TEMPERATURE, icon=ICON_THERMOMETER
)
_PUMP_SPEED_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    icon=ICON_PERCENT,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_EMPTY,
    state_class=STATE_CLASS_MEASUREMENT,
)
_OPERATING_HOURS_SCHEMA = sensor.sensor_schema(kind=sensor.DURATION_H)

# Each sensor key a model can have: (C++ setter name, schema).
SENSORS = {
    CONF_TEMPERATURE_1: ("temperature1", _TEMPERATURE_SCHEMA),
    CONF_TEMPERATURE_2: ("temperature2", _TEMPERATURE_SCHEMA),
    CONF_TEMPERATURE_3: ("temperature3", _TEMPERATURE_SCHEMA),
    CONF_TEMPERATURE_4: ("temperature4", _TEMPERATURE_SCHEMA),
    CONF_TEMPERATURE_5: ("temperature5", _TEMPERATURE_SCHEMA),
    CONF_PUMP_SPEED: ("pump_speed", _PUMP_SPEED_SCHEMA),
    CONF_PUMP_SPEED_1: ("pump_speed1", _PUMP_SPEED_SCHEMA),
    CONF_PUMP_SPEED_2: ("pump_speed2", _PUMP_SPEED_SCHEMA),
    CONF_OPERATING_HOURS: ("operating_hours", _OPERATING_HOURS_SCHEMA),
    CONF_OPERATING_HOURS_1: ("operating_hours1", _OPERATING_HOURS_SCHEMA),
    CONF_OPERATING_HOURS_2: ("operating_hours2", _OPERATING_HOURS_SCHEMA),
    CONF_HEAT_QUANTITY: (
        "heat_quantity",
        sensor.sensor_schema(
            kind=sensor.ENERGY_WH, icon=ICON_RADIATOR, accuracy_decimals=0
        ),
    ),
    CONF_TIME: (
        "time",
        sensor.sensor_schema(
            kind=sensor.DURATION_MIN,
            icon=ICON_TIMER,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    ),
    CONF_VERSION: (
        "version",
        sensor.sensor_schema(
            accuracy_decimals=2,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    ),
    CONF_FLOW_RATE: (
        "flow_rate",
        sensor.sensor_schema(
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    ),
}

_DUAL_PUMP = (
    CONF_PUMP_SPEED_1,
    CONF_PUMP_SPEED_2,
    CONF_OPERATING_HOURS_1,
    CONF_OPERATING_HOURS_2,
)
_T1_T4 = (
    CONF_TEMPERATURE_1,
    CONF_TEMPERATURE_2,
    CONF_TEMPERATURE_3,
    CONF_TEMPERATURE_4,
)

# Model: (class, VBus source address, sensors in registration order).
MODELS = {
    CONF_DELTASOL_BS_PLUS: (
        DeltaSol_BS_Plus,
        0x4221,
        (*_T1_T4, *_DUAL_PUMP, CONF_HEAT_QUANTITY, CONF_TIME, CONF_VERSION),
    ),
    CONF_DELTASOL_BS_2009: (
        DeltaSol_BS_2009,
        0x427B,
        (*_T1_T4, *_DUAL_PUMP, CONF_HEAT_QUANTITY, CONF_TIME, CONF_VERSION),
    ),
    CONF_DELTASOL_BS2: (
        DeltaSol_BS2,
        0x4278,
        (*_T1_T4, *_DUAL_PUMP, CONF_HEAT_QUANTITY, CONF_VERSION),
    ),
    CONF_DELTASOL_C: (
        DeltaSol_C,
        0x4212,
        (*_T1_T4, *_DUAL_PUMP, CONF_HEAT_QUANTITY, CONF_TIME),
    ),
    CONF_DELTASOL_CS2: (
        DeltaSol_CS2,
        0x1121,
        (
            *_T1_T4,
            CONF_PUMP_SPEED,
            CONF_OPERATING_HOURS,
            CONF_HEAT_QUANTITY,
            CONF_VERSION,
        ),
    ),
    CONF_DELTASOL_CS4: (
        DeltaSol_CS4,
        0x1122,
        (
            *_T1_T4,
            CONF_TEMPERATURE_5,
            *_DUAL_PUMP,
            CONF_HEAT_QUANTITY,
            CONF_TIME,
            CONF_VERSION,
            CONF_FLOW_RATE,
        ),
    ),
    CONF_DELTASOL_CS_PLUS: (
        DeltaSol_CS_Plus,
        0x2211,
        (
            *_T1_T4,
            CONF_TEMPERATURE_5,
            *_DUAL_PUMP,
            CONF_HEAT_QUANTITY,
            CONF_TIME,
            CONF_VERSION,
            CONF_FLOW_RATE,
        ),
    ),
}


def _model_schema(class_: cg.MockObjClass, keys: tuple[str, ...]) -> cv.Schema:
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.GenerateID(CONF_VBUS_ID): cv.use_id(VBus),
            **{cv.Optional(key): SENSORS[key][1] for key in keys},
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        **{
            model: _model_schema(class_, keys)
            for model, (class_, _, keys) in MODELS.items()
        },
        CONF_CUSTOM: cv.Schema(
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
    sensors = sensor.sub_sensors(config)

    if (model := MODELS.get(config[CONF_MODEL])) is not None:
        _, source, keys = model
        cg.add(var.set_command(0x0100))
        cg.add(var.set_source(source))
        cg.add(var.set_dest(0x0010))
        for key in keys:
            await sensors(key, getattr(var, f"set_{SENSORS[key][0]}_sensor"))

    else:
        cg.add_optional(var.set_command, config.get(CONF_COMMAND))
        cg.add_optional(var.set_source, config.get(CONF_SOURCE))
        cg.add_optional(var.set_dest, config.get(CONF_DEST))
        custom_sensors = []
        for conf in config[CONF_SENSORS]:
            sens = await sensor.new_sensor(conf)
            lambda_ = await cg.process_lambda(
                conf[CONF_LAMBDA],
                [(cg.std_vector.template(cg.uint8), "x")],
                return_type=cg.float_,
            )
            cg.add(sens.set_message_parser(lambda_))
            custom_sensors.append(sens)
        cg.add(var.set_sensors(custom_sensors))

    vbus = await cg.get_variable(config[CONF_VBUS_ID])
    cg.add(vbus.register_listener(var))
