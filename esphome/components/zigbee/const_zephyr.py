# These two schema keys live in esphome.components.const so entity base
# schemas can use them without importing this package.
from esphome.const import (  # noqa: F401  # pylint: disable=unused-import
    CONF_ZIGBEE_BINARY_SENSOR,
    CONF_ZIGBEE_ID,
    CONF_ZIGBEE_SENSOR,
)

CONF_MAX_EP_NUMBER_ZEPHYR = 8
CONF_ZIGBEE_SWITCH = "zigbee_switch"
CONF_ZIGBEE_NUMBER = "zigbee_number"
CONF_SLEEPY = "sleepy"
CONF_IEEE802154_VENDOR_OUI = "ieee802154_vendor_oui"

# Keys for CORE.data storage
KEY_EP_NUMBER = "ep_number"

# External ZBOSS SDK types (just strings for codegen)
ZB_ZCL_BASIC_ATTRS_EXT_T = "zb_zcl_basic_attrs_ext_t"
ZB_ZCL_IDENTIFY_ATTRS_T = "zb_zcl_identify_attrs_t"

# Cluster IDs
ZB_ZCL_CLUSTER_ID_BASIC = "ZB_ZCL_CLUSTER_ID_BASIC"
ZB_ZCL_CLUSTER_ID_IDENTIFY = "ZB_ZCL_CLUSTER_ID_IDENTIFY"
ZB_ZCL_CLUSTER_ID_BINARY_INPUT = "ZB_ZCL_CLUSTER_ID_BINARY_INPUT"
ZB_ZCL_CLUSTER_ID_ANALOG_INPUT = "ZB_ZCL_CLUSTER_ID_ANALOG_INPUT"
ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT = "ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT"
ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT = "ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT"
