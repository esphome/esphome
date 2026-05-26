from typing import Any

import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE,
    CONF_ID,
    CONF_LAMBDA,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_LUX,
    UNIT_PERCENT,
)
from esphome.core import Lambda

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
    SCALE,
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
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONF_DEVICE: None,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x51,
                        CONF_TYPE: "BOOL",
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x6F,
                        CONF_TYPE: "MAP8",
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x1C,
                        CONF_TYPE: "STRING",
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
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONF_DEVICE: None,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x51,
                        CONF_TYPE: "BOOL",
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x6F,
                        CONF_TYPE: "MAP8",
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x1C,
                        CONF_TYPE: "STRING",
                    },
                ],
            },
        ],
    },
    DEVICE_CLASS_TEMPERATURE: {
        CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
        DEVICE_TYPE: "TEMPERATURE_SENSOR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "TEMPERATURE_MEASUREMENT",
                ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "INT16",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        SCALE: 100,
                        CONF_DEVICE: None,
                    },
                ],
            },
        ],
    },
    DEVICE_CLASS_HUMIDITY: {
        CONF_UNIT_OF_MEASUREMENT: UNIT_PERCENT,
        DEVICE_TYPE: "CUSTOM_ATTR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "REL_HUMIDITY_MEASUREMENT",
                ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "UINT16",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        SCALE: 100,
                        CONF_DEVICE: None,
                    },
                ],
            },
        ],
    },
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE: {
        CONF_UNIT_OF_MEASUREMENT: UNIT_HECTOPASCAL,
        DEVICE_TYPE: "CUSTOM_ATTR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "PRESSURE_MEASUREMENT",
                ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "INT16",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONF_DEVICE: None,
                    },
                ],
            },
        ],
    },
    DEVICE_CLASS_PRESSURE: {
        CONF_UNIT_OF_MEASUREMENT: UNIT_HECTOPASCAL,
        DEVICE_TYPE: "PRESSURE_SENSOR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "PRESSURE_MEASUREMENT",
                ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "INT16",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONF_DEVICE: None,
                    },
                ],
            },
        ],
    },
    DEVICE_CLASS_ILLUMINANCE: {
        CONF_UNIT_OF_MEASUREMENT: UNIT_LUX,
        DEVICE_TYPE: "CUSTOM_ATTR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "ILLUMINANCE_MEASUREMENT",
                ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "UINT16",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONF_LAMBDA: cv.lambda_(Lambda("return log10(x)*10000 + 1;")),
                        CONF_DEVICE: None,
                    },
                ],
            },
        ],
    },
    "on_off": {
        DEVICE_TYPE: "ON_OFF_OUTPUT",
        CONF_CLUSTERS: [
            {
                CONF_ID: "ON_OFF",
                ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONF_DEVICE: None,
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
