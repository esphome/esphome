import esphome.codegen as cg
from esphome.core import ID
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    CONF_OFFSET,
)

from . import (
    FlagDesc,
    TextSensorDesc,
    BmsComponent,
    CONF_BMS_ID,
    CONF_MSG_ID,
    CONF_WARNINGS,
    CONF_ALARMS,
    CONF_BIT_NO,
)

ICON_CAR_BATTERY = "mdi:car-battery"
SMA_MSG_ID = 0x35A
PYLON_MSG_ID = 0x359
CONF_WARN_OFFSET = "warn_offset"
CONF_WARN_BIT_NO = "warn_bit_no"


def bms_text_desc(msg_id=-1):
    return {
        CONF_MSG_ID: msg_id,
    }


# define an alarm or warning bit, found as a bit in a byte at an offset in a message
def bit_desc(msg_id, offset, bitno, warn_offset, warn_bitno):
    return {
        CONF_MSG_ID: msg_id,
        CONF_OFFSET: offset,
        CONF_BIT_NO: bitno,
        CONF_WARN_OFFSET: warn_offset,
        CONF_WARN_BIT_NO: warn_bitno,
    }


TEXT_SENSORS = {
    "bms_name": (bms_text_desc(0x35E),),  # maps directly to a CAN message
    CONF_ALARMS: (bms_text_desc(),),  # Synthesised from alarm bits
    CONF_WARNINGS: (bms_text_desc(),),
}

ALARMS = {
    "general_alarm": (bit_desc(SMA_MSG_ID, 0, 0, 4, 0),),
    "high_voltage": (
        bit_desc(SMA_MSG_ID, 0, 2, 4, 2),
        bit_desc(PYLON_MSG_ID, 2, 1, 0, 1),
    ),
    "low_voltage": (
        bit_desc(SMA_MSG_ID, 0, 4, 4, 4),
        bit_desc(PYLON_MSG_ID, 2, 2, 0, 2),
    ),
    "high_temperature": (
        bit_desc(SMA_MSG_ID, 0, 6, 4, 6),
        bit_desc(PYLON_MSG_ID, 2, 3, 0, 3),
    ),
    "low_temperature": (
        bit_desc(SMA_MSG_ID, 1, 0, 5, 0),
        bit_desc(PYLON_MSG_ID, 2, 4, 0, 4),
    ),
    "high_temperature_charge": (bit_desc(SMA_MSG_ID, 1, 2, 5, 2),),
    "low_temperature_charge": (bit_desc(SMA_MSG_ID, 1, 4, 5, 4),),
    "high_current": (
        bit_desc(SMA_MSG_ID, 1, 6, 5, 6),
        bit_desc(PYLON_MSG_ID, 2, 7, 0, 7),
    ),
    "high_current_charge": (
        bit_desc(SMA_MSG_ID, 2, 0, 6, 0),
        bit_desc(PYLON_MSG_ID, 3, 0, 1, 0),
    ),
    "contactor_error": (bit_desc(SMA_MSG_ID, 2, 2, 6, 2),),
    "short_circuit": (bit_desc(SMA_MSG_ID, 2, 4, 6, 4),),
    "bms_internal_error": (
        bit_desc(SMA_MSG_ID, 2, 6, 6, 6),
        bit_desc(PYLON_MSG_ID, 3, 3, 1, 6),
    ),
    "cell_imbalance": (bit_desc(SMA_MSG_ID, 3, 0, 7, 0),),
}


def text_schema(name):
    return (
        cv.Optional(name),
        text_sensor.text_sensor_schema(
            icon=ICON_CAR_BATTERY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    )


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_BMS_ID): cv.use_id(BmsComponent),
        }
    )
    .extend(dict(map(text_schema, TEXT_SENSORS)))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    bms_id = config[CONF_BMS_ID]
    hub = await cg.get_variable(bms_id)
    vectors = {}  # map message ids to vectors.
    index = 0
    for key, entries in ALARMS.items():
        for entry in entries:
            msg_id = entry[CONF_MSG_ID]
            if msg_id not in vectors:
                vectors[msg_id] = cg.new_Pvariable(
                    ID(
                        f"flag_msg_ids_{bms_id}_{msg_id}",
                        True,
                        cg.std_vector.template(FlagDesc.operator("ptr")),
                    )
                )
            vector = vectors[msg_id]
            cg.add(
                vector.push_back(
                    FlagDesc.new(
                        key,
                        key.title().replace(
                            "_", " "
                        ),  # convert key to title case phrase.
                        entry[CONF_MSG_ID],
                        entry[CONF_OFFSET],
                        entry[CONF_BIT_NO],
                        entry[CONF_WARN_OFFSET],
                        entry[CONF_WARN_BIT_NO],
                        1 << index,
                    )
                )
            )
        index += 1
    for id, vector in vectors.items():
        cg.add(hub.add_flag_list(id, vector))
    vectors = {}  # map message ids to vectors.
    for key, list in TEXT_SENSORS.items():
        if key in config:
            conf = config[key]
            for entry in list:
                msg_id = entry[CONF_MSG_ID]
                if msg_id >= 0:
                    sensor = await text_sensor.new_text_sensor(conf)
                    if msg_id != -1 and msg_id not in vectors:
                        vectors[msg_id] = cg.new_Pvariable(
                            ID(
                                f"text_msg_ids_{bms_id}_{msg_id}",
                                True,
                                cg.std_vector.template(TextSensorDesc.operator("ptr")),
                            )
                        )
                    vector = vectors[msg_id]
                    cg.add(
                        vector.push_back(
                            TextSensorDesc.new(
                                sensor,
                                msg_id,
                            )
                        )
                    )
    for id, vector in vectors.items():
        cg.add(hub.add_text_sensor_list(id, vector))
