from typing import Any

import esphome.config_validation as cv
from esphome.const import CONF_DEVICE, CONF_ID, CONF_TYPE

from .const import CONF_REPORT, REPORT
from .const_esp32 import (
    CLUSTER_ROLE,
    CONF_ATTRIBUTE_ID,
    CONF_ATTRIBUTES,
    CONF_CLUSTERS,
    CONF_MAX_EP_NUMBER,
    CONF_NUM,
    DEVICE_TYPE,
    ROLE,
)

# endpoint configs:
ep_configs: dict[str, dict[str, Any]] = {
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
                        CONF_REPORT: REPORT["enable"],
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
    "analog_input": {
        DEVICE_TYPE: "CUSTOM_ATTR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "ANALOG_INPUT",
                ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x55,
                        CONF_TYPE: "SINGLE",
                        CONF_REPORT: REPORT["enable"],
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


def create_ep(ep_list: list[dict[str, Any]], router: bool) -> list[dict[str, Any]]:
    # create dummy endpoint if list is empty
    if not ep_list:
        ep_type = "CUSTOM_ATTR"
        if router:
            ep_type = "RANGE_EXTENDER"
        ep_list = [
            {
                DEVICE_TYPE: ep_type,
            }
        ]
    # enumerate endpoints
    for i, ep in enumerate(ep_list, 1):
        ep[CONF_NUM] = i
    if len(ep_list) > CONF_MAX_EP_NUMBER:
        raise cv.Invalid(
            f"Too many devices. Zigbee can define only {CONF_MAX_EP_NUMBER} endpoints."
        )
    return ep_list
