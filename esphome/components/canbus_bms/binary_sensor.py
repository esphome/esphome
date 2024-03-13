import esphome.codegen as cg
from esphome.core import ID
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
    CONF_OFFSET,
    CONF_FILTERS,
)

from . import (
    BmsComponent,
    BinarySensorDesc,
    CONF_BMS_ID,
    CONF_MSG_ID,
    CONF_BIT_NO,
    CONF_WARNINGS,
    CONF_ALARMS,
)


# define an alarm or warning bit, found as a bit in a byte at an offset in a message
def bms_bit_desc(msg_id=-1, offset=-1, bitno=-1):
    return {
        CONF_MSG_ID: msg_id,
        CONF_OFFSET: offset,
        CONF_BIT_NO: bitno,
    }


PYLON_REQUEST_MSG_ID = 0x35C

# The sensor map from conf id to message and data decoding information. Each sensor may have multiple
# implementations corresponding to different BMS protocols.
REQUESTS = {
    "charge_enable": (bms_bit_desc(PYLON_REQUEST_MSG_ID, 0, 7),),
    "discharge_enable": (bms_bit_desc(PYLON_REQUEST_MSG_ID, 0, 6),),
    "force_charge_1": (bms_bit_desc(PYLON_REQUEST_MSG_ID, 0, 5),),
    "force_charge_2": (bms_bit_desc(PYLON_REQUEST_MSG_ID, 0, 4),),
    "request_full_charge": (bms_bit_desc(PYLON_REQUEST_MSG_ID, 0, 3),),
}

FLAGS = {
    CONF_ALARMS,
    CONF_WARNINGS,
}


def binary_schema(name):
    return (
        cv.Optional(name),
        binary_sensor.binary_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    )


def flag_schema(name):
    return (
        cv.Optional(name),
        binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    )


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_BMS_ID): cv.use_id(BmsComponent),
        }
    )
    .extend(dict(map(binary_schema, REQUESTS)))
    .extend(dict(map(flag_schema, FLAGS)))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    bms_id = config[CONF_BMS_ID]
    hub = await cg.get_variable(bms_id)
    # Add entries for sensors with direct bit mappings
    vectors = {}  # map message ids to vectors.
    for key, entries in REQUESTS.items():
        sens = cg.nullptr
        filtered = True
        if key in config:
            conf = config[key]
            sens = await binary_sensor.new_binary_sensor(conf)
            filtered = CONF_FILTERS in conf
        for desc in entries:
            msg_id = desc[CONF_MSG_ID]
            if msg_id not in vectors:
                vectors[msg_id] = cg.new_Pvariable(
                    ID(
                        f"binary_msg_ids_{bms_id}_{msg_id}",
                        True,
                        cg.std_vector.template(BinarySensorDesc.operator("ptr")),
                    )
                )
            vector = vectors[msg_id]
            cg.add(
                vector.push_back(
                    BinarySensorDesc.new(
                        key,
                        sens,
                        msg_id,
                        desc[CONF_OFFSET],
                        desc[CONF_BIT_NO],
                        filtered,
                    )
                )
            )
    for id, vector in vectors.items():
        cg.add(hub.add_binary_sensor_list(id, vector))

    # Add binary sensors that are not directly mapped in CAN bus messages.
    if CONF_ALARMS in config:
        sensor = await binary_sensor.new_binary_sensor(config[CONF_ALARMS])
        cg.add(hub.set_alarm_binary_sensor(sensor))
    if CONF_WARNINGS in config:
        sensor = await binary_sensor.new_binary_sensor(config[CONF_WARNINGS])
        cg.add(hub.set_warning_binary_sensor(sensor))
