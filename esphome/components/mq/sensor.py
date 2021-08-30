import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.core import CORE
from esphome.const import (
    CONF_ID,
    CONF_PIN,
    CONF_MODEL,
    DEVICE_CLASS_CARBON_MONOXIDE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

CONF_RL = "rl"
CONF_R0 = "r0"
CONF_SENSOR_ACETONA = "sensor_acetona"
CONF_SENSOR_ALCOHOL = "sensor_alcohol"
CONF_SENSOR_BENZENE = "sensor_benzene"
CONF_SENSOR_CH4 = "sensor_ch4"
CONF_SENSOR_CL2 = "sensor_cl2"
CONF_SENSOR_CO = "sensor_co"
CONF_SENSOR_CO2 = "sensor_co2"
CONF_SENSOR_ETHANOL = "sensor_ethanol"
CONF_SENSOR_H2 = "sensor_h2"
CONF_SENSOR_HEXANE = "sensor_hexane"
CONF_SENSOR_HYDROGEN = "sensor_hydrogen"
CONF_SENSOR_ISO_BUTANO = "sensor_iso_butano"
CONF_SENSOR_LPG = "sensor_lpg"
CONF_SENSOR_NH4 = "sensor_nh4"
CONF_SENSOR_NOX = "sensor_nox"
CONF_SENSOR_O3 = "sensor_o3"
CONF_SENSOR_PROPANE = "sensor_propane"
CONF_SENSOR_SMOKE = "sensor_smoke"
CONF_SENSOR_TOLUENO = "sensor_tolueno"

mq_ns = cg.esphome_ns.namespace("mq")
MQModel = mq_ns.enum("MQModel")
MQ_MODELS = {
    "MQ2": MQModel.MQ_MODEL_2,
    "MQ3": MQModel.MQ_MODEL_3,
    "MQ4": MQModel.MQ_MODEL_4,
    "MQ5": MQModel.MQ_MODEL_5,
    "MQ6": MQModel.MQ_MODEL_6,
    "MQ7": MQModel.MQ_MODEL_7,
    "MQ8": MQModel.MQ_MODEL_8,
    "MQ9": MQModel.MQ_MODEL_9,
    "MQ135": MQModel.MQ_MODEL_135,
}
MQGasType = mq_ns.enum("MQ_GAS_TYPES")

MQ_MODEL_SENSORS = {
    "MQ2": [CONF_SENSOR_H2, CONF_SENSOR_LPG, CONF_SENSOR_CO, CONF_SENSOR_ALCOHOL, CONF_SENSOR_PROPANE],
    "MQ3": [CONF_SENSOR_LPG, CONF_SENSOR_CH4, CONF_SENSOR_CO, CONF_SENSOR_ALCOHOL, CONF_SENSOR_BENZENE, CONF_SENSOR_HEXANE],
    "MQ4": [CONF_SENSOR_LPG, CONF_SENSOR_CH4, CONF_SENSOR_CO, CONF_SENSOR_ALCOHOL, CONF_SENSOR_SMOKE],
    "MQ5": [CONF_SENSOR_H2, CONF_SENSOR_LPG, CONF_SENSOR_CH4, CONF_SENSOR_CO, CONF_SENSOR_ALCOHOL],
    "MQ6": [CONF_SENSOR_H2, CONF_SENSOR_LPG, CONF_SENSOR_CH4, CONF_SENSOR_CO, CONF_SENSOR_ALCOHOL],
    "MQ7": [CONF_SENSOR_H2, CONF_SENSOR_LPG, CONF_SENSOR_CH4, CONF_SENSOR_CO, CONF_SENSOR_ALCOHOL],
    "MQ8": [CONF_SENSOR_H2, CONF_SENSOR_LPG, CONF_SENSOR_CH4, CONF_SENSOR_CO, CONF_SENSOR_ALCOHOL],
    "MQ9": [CONF_SENSOR_LPG, CONF_SENSOR_CH4, CONF_SENSOR_CO],
    "MQ135": [CONF_SENSOR_CO, CONF_SENSOR_ALCOHOL, CONF_SENSOR_CO2, CONF_SENSOR_TOLUENO, CONF_SENSOR_NH4, CONF_SENSOR_ACETONA],
}

MQ_GAS_TYPES = {
    CONF_SENSOR_ACETONA: MQGasType.MQ_GAS_TYPE_ACETONA,
    CONF_SENSOR_ALCOHOL: MQGasType.MQ_GAS_TYPE_ALCOHOL,
    CONF_SENSOR_BENZENE: MQGasType.MQ_GAS_TYPE_BENZENE,
    CONF_SENSOR_CH4: MQGasType.MQ_GAS_TYPE_CH4,
    CONF_SENSOR_CL2: MQGasType.MQ_GAS_TYPE_CL2,
    CONF_SENSOR_CO: MQGasType.MQ_GAS_TYPE_CO,
    CONF_SENSOR_CO2: MQGasType.MQ_GAS_TYPE_CO2,
    CONF_SENSOR_ETHANOL: MQGasType.MQ_GAS_TYPE_ETHANOL,
    CONF_SENSOR_H2: MQGasType.MQ_GAS_TYPE_H2,
    CONF_SENSOR_HEXANE: MQGasType.MQ_GAS_TYPE_HEXANE,
    CONF_SENSOR_HYDROGEN: MQGasType.MQ_GAS_TYPE_HYDROGEN,
    CONF_SENSOR_ISO_BUTANO: MQGasType.MQ_GAS_TYPE_ISO_BUTANO,
    CONF_SENSOR_LPG: MQGasType.MQ_GAS_TYPE_LPG,
    CONF_SENSOR_NH4: MQGasType.MQ_GAS_TYPE_NH4,
    CONF_SENSOR_NOX: MQGasType.MQ_GAS_TYPE_NOX,
    CONF_SENSOR_O3: MQGasType.MQ_GAS_TYPE_O3,
    CONF_SENSOR_PROPANE: MQGasType.MQ_GAS_TYPE_PROPANE,
    CONF_SENSOR_SMOKE: MQGasType.MQ_GAS_TYPE_SMOKE,
    CONF_SENSOR_TOLUENO: MQGasType.MQ_GAS_TYPE_TOLUENO,
}

MQSensor = mq_ns.class_("MQSensor", cg.PollingComponent)


def get_model_valid_sensors(config):
    return set(MQ_MODEL_SENSORS[config[CONF_MODEL]])


def get_sensors_schemas(config):
    return set(list(dict(filter(lambda elem: "sensor_" in elem[0].lower(), config.items())).keys()))


def validate_sensors(config):
    model = config[CONF_MODEL]
    valid_sensors = get_model_valid_sensors(config)
    sensors_schemas = get_sensors_schemas(config)
    if sensors_schemas - valid_sensors != set():
        invalid_sensors = sensors_schemas.difference(valid_sensors)
        raise cv.Invalid(
            "Invalid sensors definition for {model}\nNot supported sensors:\n\t{}\n{model} supports:\n\t{}"
            "".format(", ".join(map(str, invalid_sensors)), ", ".join(map(str, valid_sensors)), model=model)
        )
    return config


sensor_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_PARTS_PER_MILLION,
    icon=ICON_MOLECULE_CO2,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_CARBON_MONOXIDE,
    state_class=STATE_CLASS_MEASUREMENT
)

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(MQSensor),
        cv.Required(CONF_PIN): pins.internal_gpio_analog_pin_schema,
        cv.Optional(CONF_RL, default=10.0): cv.positive_float,
        cv.Optional(CONF_R0): cv.positive_float,
        cv.Required(CONF_MODEL): cv.enum(
            MQ_MODELS, upper=True, space="_"
        ),
        cv.Optional(CONF_SENSOR_ACETONA): sensor_schema,
        cv.Optional(CONF_SENSOR_ALCOHOL): sensor_schema,
        cv.Optional(CONF_SENSOR_BENZENE): sensor_schema,
        cv.Optional(CONF_SENSOR_CH4): sensor_schema,
        cv.Optional(CONF_SENSOR_CL2): sensor_schema,
        cv.Optional(CONF_SENSOR_CO): sensor_schema,
        cv.Optional(CONF_SENSOR_CO2): sensor_schema,
        cv.Optional(CONF_SENSOR_ETHANOL): sensor_schema,
        cv.Optional(CONF_SENSOR_H2): sensor_schema,
        cv.Optional(CONF_SENSOR_HEXANE): sensor_schema,
        cv.Optional(CONF_SENSOR_HYDROGEN): sensor_schema,
        cv.Optional(CONF_SENSOR_ISO_BUTANO): sensor_schema,
        cv.Optional(CONF_SENSOR_LPG): sensor_schema,
        cv.Optional(CONF_SENSOR_NH4): sensor_schema,
        cv.Optional(CONF_SENSOR_NOX): sensor_schema,
        cv.Optional(CONF_SENSOR_O3): sensor_schema,
        cv.Optional(CONF_SENSOR_PROPANE): sensor_schema,
        cv.Optional(CONF_SENSOR_SMOKE): sensor_schema,
        cv.Optional(CONF_SENSOR_TOLUENO): sensor_schema,
    }).extend(cv.polling_component_schema("10s")),
    validate_sensors,
)


async def to_code(config):
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin, config[CONF_MODEL], CORE.is_esp8266, config[CONF_RL])
    await cg.register_component(var, config)

    cg.add_library("MQUnifiedsensor", "2.0.1")

    if CONF_R0 in config:
        cg.add(var.set_R0(config[CONF_R0]))

    model_valid_sensors = set(MQ_MODEL_SENSORS[config[CONF_MODEL]])
    sensors_schemas = get_sensors_schemas(config)
    sensors = model_valid_sensors.intersection(sensors_schemas)

    for item in sensors:
        conf = config[item]
        gas_type = MQ_GAS_TYPES[item]
        sens = await sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, gas_type))
