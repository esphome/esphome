import esphome.config_validation as cv
from esphome.const import CONF_DEVICE, CONF_ID, CONF_TYPE

from .const import CONF_REPORT, REPORT
from .const_esp32 import (
    CLUSTER_ROLE,
    CONF_ATTRIBUTE_ID,
    CONF_ATTRIBUTES,
    CONF_CLUSTERS,
    CONF_NUM,
    DEVICE_TYPE,
    ROLE,
)

# endpoint configs:
ep_configs = {
    "binary_input": {
        DEVICE_TYPE: "SIMPLE_SENSOR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "BINARY_INPUT",
                ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x55,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: REPORT["yes"],
                        CONF_DEVICE: None,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x51,
                        CONF_TYPE: "BOOL",
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x6F,
                        CONF_TYPE: "8BITMAP",
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x1C,
                        CONF_TYPE: "CHAR_STRING",
                    },
                ],
            },
        ],
    },
}


def get_next_ep_num(ep_nums):
    # get next free number
    try:
        ep_num = [i for i in range(1, 240) if i not in ep_nums][0]
        ep_nums.append(ep_num)
    except IndexError as e:
        raise cv.Invalid(
            "Too many devices. Zigbee can define only 240 endpoints."
        ) from e
    return ep_num


def create_ep(ep_list):
    # create dummy endpoint if list is empty
    if not ep_list:
        ep_list = [
            {
                DEVICE_TYPE: "CUSTOM_ATTR",
            }
        ]
    # enumerate endpoints
    for i, ep in enumerate(ep_list, 1):
        ep[CONF_NUM] = i
    return ep_list
