import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_BINARY_SENSORS,
    CONF_COMMAND,
    CONF_CUSTOM,
    CONF_DEST,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_SOURCE,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from .. import (
    vbus_ns,
    VBus,
    CONF_VBUS_ID,
    CONF_DELTASOL_BS_PLUS,
    CONF_DELTASOL_C,
    CONF_DELTASOL_CS2,
    CONF_DELTASOL_CS_PLUS,
)

DeltaSol_BS_Plus = vbus_ns.class_("DeltaSolBSPlusBSensor", cg.Component)
DeltaSol_C = vbus_ns.class_("DeltaSolCBSensor", cg.Component)
DeltaSol_CS2 = vbus_ns.class_("DeltaSolCS2BSensor", cg.Component)
DeltaSol_CS_Plus = vbus_ns.class_("DeltaSolCSPlusBSensor", cg.Component)
VBusCustom = vbus_ns.class_("VBusCustomBSensor", cg.Component)
VBusCustomSub = vbus_ns.class_("VBusCustomSubBSensor", cg.Component)

CONF_RELAY1 = "relay1"
CONF_RELAY2 = "relay2"
CONF_SENSOR1_ERROR = "sensor1_error"
CONF_SENSOR2_ERROR = "sensor2_error"
CONF_SENSOR3_ERROR = "sensor3_error"
CONF_SENSOR4_ERROR = "sensor4_error"
CONF_COLLECTOR_MAX = "collector_max"
CONF_COLLECTOR_MIN = "collector_min"
CONF_COLLECTOR_FROST = "collector_frost"
CONF_TUBE_COLLECTOR = "tube_collector"
CONF_RECOOLING = "recooling"
CONF_HQM = "hqm"

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_DELTASOL_BS_PLUS: cv.COMPONENT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(DeltaSol_BS_Plus),
                cv.GenerateID(CONF_VBUS_ID): cv.use_id(VBus),
                cv.Optional(CONF_RELAY1): binary_sensor.binary_sensor_schema(),
                cv.Optional(CONF_RELAY2): binary_sensor.binary_sensor_schema(),
                cv.Optional(CONF_SENSOR1_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR2_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR3_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR4_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_COLLECTOR_MAX): binary_sensor.binary_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_COLLECTOR_MIN): binary_sensor.binary_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_COLLECTOR_FROST): binary_sensor.binary_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_TUBE_COLLECTOR): binary_sensor.binary_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_RECOOLING): binary_sensor.binary_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_HQM): binary_sensor.binary_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
            }
        ),
        CONF_DELTASOL_C: cv.COMPONENT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(DeltaSol_C),
                cv.GenerateID(CONF_VBUS_ID): cv.use_id(VBus),
                cv.Optional(CONF_SENSOR1_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR2_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR3_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR4_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
            }
        ),
        CONF_DELTASOL_CS2: cv.COMPONENT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(DeltaSol_CS2),
                cv.GenerateID(CONF_VBUS_ID): cv.use_id(VBus),
                cv.Optional(CONF_SENSOR1_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR2_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR3_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR4_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
            }
        ),
        CONF_DELTASOL_CS_PLUS: cv.COMPONENT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(DeltaSol_CS_Plus),
                cv.GenerateID(CONF_VBUS_ID): cv.use_id(VBus),
                cv.Optional(CONF_SENSOR1_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR2_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR3_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_SENSOR4_ERROR): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
            }
        ),
        CONF_CUSTOM: cv.COMPONENT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(VBusCustom),
                cv.GenerateID(CONF_VBUS_ID): cv.use_id(VBus),
                cv.Optional(CONF_COMMAND): cv.uint16_t,
                cv.Optional(CONF_SOURCE): cv.uint16_t,
                cv.Optional(CONF_DEST): cv.uint16_t,
                cv.Optional(CONF_BINARY_SENSORS): cv.ensure_list(
                    binary_sensor.binary_sensor_schema().extend(
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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if config[CONF_MODEL] == CONF_DELTASOL_BS_PLUS:
        cg.add(var.set_command(0x0100))
        cg.add(var.set_source(0x4221))
        cg.add(var.set_dest(0x0010))
        if CONF_RELAY1 in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_RELAY1])
            cg.add(var.set_relay1_bsensor(sens))
        if CONF_RELAY2 in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_RELAY2])
            cg.add(var.set_relay2_bsensor(sens))
        if CONF_SENSOR1_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR1_ERROR])
            cg.add(var.set_s1_error_bsensor(sens))
        if CONF_SENSOR2_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR2_ERROR])
            cg.add(var.set_s2_error_bsensor(sens))
        if CONF_SENSOR3_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR3_ERROR])
            cg.add(var.set_s3_error_bsensor(sens))
        if CONF_SENSOR4_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR4_ERROR])
            cg.add(var.set_s4_error_bsensor(sens))
        if CONF_COLLECTOR_MAX in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_COLLECTOR_MAX])
            cg.add(var.set_collector_max_bsensor(sens))
        if CONF_COLLECTOR_MIN in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_COLLECTOR_MIN])
            cg.add(var.set_collector_min_bsensor(sens))
        if CONF_COLLECTOR_FROST in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_COLLECTOR_FROST])
            cg.add(var.set_collector_frost_bsensor(sens))
        if CONF_TUBE_COLLECTOR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_TUBE_COLLECTOR])
            cg.add(var.set_tube_collector_bsensor(sens))
        if CONF_RECOOLING in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_RECOOLING])
            cg.add(var.set_recooling_bsensor(sens))
        if CONF_HQM in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_HQM])
            cg.add(var.set_hqm_bsensor(sens))

    elif config[CONF_MODEL] == CONF_DELTASOL_C:
        cg.add(var.set_command(0x0100))
        cg.add(var.set_source(0x4212))
        cg.add(var.set_dest(0x0010))
        if CONF_SENSOR1_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR1_ERROR])
            cg.add(var.set_s1_error_bsensor(sens))
        if CONF_SENSOR2_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR2_ERROR])
            cg.add(var.set_s2_error_bsensor(sens))
        if CONF_SENSOR3_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR3_ERROR])
            cg.add(var.set_s3_error_bsensor(sens))
        if CONF_SENSOR4_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR4_ERROR])
            cg.add(var.set_s4_error_bsensor(sens))

    elif config[CONF_MODEL] == CONF_DELTASOL_CS2:
        cg.add(var.set_command(0x0100))
        cg.add(var.set_source(0x1121))
        cg.add(var.set_dest(0x0010))
        if CONF_SENSOR1_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR1_ERROR])
            cg.add(var.set_s1_error_bsensor(sens))
        if CONF_SENSOR2_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR2_ERROR])
            cg.add(var.set_s2_error_bsensor(sens))
        if CONF_SENSOR3_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR3_ERROR])
            cg.add(var.set_s3_error_bsensor(sens))
        if CONF_SENSOR4_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR4_ERROR])
            cg.add(var.set_s4_error_bsensor(sens))

    elif config[CONF_MODEL] == CONF_DELTASOL_CS_PLUS:
        cg.add(var.set_command(0x0100))
        cg.add(var.set_source(0x2211))
        cg.add(var.set_dest(0x0010))
        if CONF_SENSOR1_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR1_ERROR])
            cg.add(var.set_s1_error_bsensor(sens))
        if CONF_SENSOR2_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR2_ERROR])
            cg.add(var.set_s2_error_bsensor(sens))
        if CONF_SENSOR3_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR3_ERROR])
            cg.add(var.set_s3_error_bsensor(sens))
        if CONF_SENSOR4_ERROR in config:
            sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR4_ERROR])
            cg.add(var.set_s4_error_bsensor(sens))

    elif config[CONF_MODEL] == CONF_CUSTOM:
        if CONF_COMMAND in config:
            cg.add(var.set_command(config[CONF_COMMAND]))
        if CONF_SOURCE in config:
            cg.add(var.set_source(config[CONF_SOURCE]))
        if CONF_DEST in config:
            cg.add(var.set_dest(config[CONF_DEST]))
        bsensors = []
        for conf in config[CONF_BINARY_SENSORS]:
            bsens = await binary_sensor.new_binary_sensor(conf)
            lambda_ = await cg.process_lambda(
                conf[CONF_LAMBDA],
                [(cg.std_vector.template(cg.uint8), "x")],
                return_type=cg.bool_,
            )
            cg.add(bsens.set_message_parser(lambda_))
            bsensors.append(bsens)
        cg.add(var.set_bsensors(bsensors))

    vbus = await cg.get_variable(config[CONF_VBUS_ID])
    cg.add(vbus.register_listener(var))
