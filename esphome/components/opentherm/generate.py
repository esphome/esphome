import re
from typing import Optional

import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID

CONF_MESSAGE_ID = "message"
CONF_KEEP_UPDATED_ID = "keep_updated"
CONF_MESSAGE_DATA_ID = "message_data"


def get_type(entity) -> Optional[cg.RawExpression]:
    message_data: str = entity['message_data']
    if message_data.startswith('flag'):
        return cg.RawExpression('bool')
    if message_data.startswith('s'):
        nums = [int(s) for s in re.findall(r'\d+', message_data)]
        return cg.RawExpression(f'int{nums[0]}_t')
    if message_data.startswith('u'):
        nums = [int(s) for s in re.findall(r'\d+', message_data)]
        return cg.RawExpression(f'uint{nums[0]}_t')
    if message_data.startswith('f'):
        return cg.RawExpression('float')
    return None


def opentherm_schema(entity):
    return cv.Schema({
        cv.Optional(CONF_MESSAGE_ID, default=entity['message']): cv.string,
        cv.Optional(CONF_KEEP_UPDATED_ID, default=entity['keep_updated']): cv.boolean,
        cv.Optional(CONF_MESSAGE_DATA_ID, default=entity['message_data']): cv.string
    })


def create_opentherm_component(config):
    return cg.new_Pvariable(
        config[CONF_ID],
        cg.RawExpression(f'OpenThermMessageID::{config[CONF_MESSAGE_ID]}'),
        config[CONF_KEEP_UPDATED_ID],
        cg.RawExpression(f'parse_{config[CONF_MESSAGE_DATA_ID]}'),
        cg.RawExpression(f'write_{config[CONF_MESSAGE_DATA_ID]}')
    )
