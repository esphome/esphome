from typing import Any

import esphome.config_validation as cv
from esphome.const import CONF_DEVICE, CONF_ID, CONF_TYPE
from esphome.core import CORE

from .const import (
    CONF_MAX_EP_NUMBER,
    CONF_REPORT,
    CONF_USE_DEVICE_TYPE,
    KEY_ZIGBEE,
    REPORT,
)
from .const_esp32 import (
    CONF_ATTRIBUTE_ID,
    CONF_ATTRIBUTES,
    CONF_CLUSTERS,
    DEVICE_TYPE,
    KEY_ZIGBEE_EP,
    KEY_ZIGBEE_EP_NO_NUM,
    ROLE,
)

# endpoint configs:
ep_configs: dict[str, dict[str, Any]] = {
    "binary_input": {
        DEVICE_TYPE: "SIMPLE_SENSOR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "BINARY_INPUT",
                ROLE: "SERVER",
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
        CONF_CLUSTERS: [
            {
                CONF_ID: "ANALOG_INPUT",
                ROLE: "SERVER",
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
}


def get_next_ep_num(eps: list[int]) -> int:
    try:
        ep_num = [i for i in range(1, CONF_MAX_EP_NUMBER + 1) if i not in eps][0]
        eps.append(ep_num)
    except IndexError as e:
        raise cv.Invalid(
            f"Too many devices. Zigbee can define only {CONF_MAX_EP_NUMBER} endpoints."
        ) from e
    return ep_num


def merge_endpoint(
    existing_ep: dict[str, Any],
    ep_num: int | None,
    ep: dict[str, Any],
    use_type: bool | None,
    skip_error: bool,
) -> bool:
    add = True
    existing_clusters = [(cl[CONF_ID], cl[ROLE]) for cl in existing_ep[CONF_CLUSTERS]]
    for cl in [(cl[CONF_ID], cl[ROLE]) for cl in ep[CONF_CLUSTERS]]:
        if cl in existing_clusters:
            if not skip_error:
                raise cv.Invalid(
                    f"Endpoint {ep_num} has more than one cluster with cluster id {cl[0]} and role {cl[1]}."
                )
            add = False
            break
    if not add:
        return False
    if (
        use_type
        and existing_ep.get(CONF_USE_DEVICE_TYPE)
        and ep.get(DEVICE_TYPE) != existing_ep.get(DEVICE_TYPE)
    ):
        if not skip_error:
            raise cv.Invalid(
                f"Endpoint {ep_num} has a conflicting device type {existing_ep.get(DEVICE_TYPE, 'CUSTOM_ATTR')} and use_type is set for both."
            )
        return False
    if use_type:
        existing_ep[CONF_USE_DEVICE_TYPE] = use_type
        if ep.get(DEVICE_TYPE):
            existing_ep[DEVICE_TYPE] = ep[DEVICE_TYPE]
        else:
            existing_ep.pop(DEVICE_TYPE, None)
        existing_ep[CONF_CLUSTERS].extend(ep[CONF_CLUSTERS])
        return True
    if existing_ep.get(CONF_USE_DEVICE_TYPE):
        existing_ep[CONF_CLUSTERS].extend(ep[CONF_CLUSTERS])
        return True
    if (
        ep.get(DEVICE_TYPE)
        and existing_ep.get(DEVICE_TYPE)
        and ep[DEVICE_TYPE] != existing_ep[DEVICE_TYPE]
    ):
        if not skip_error:
            raise cv.Invalid(
                f"Endpoint {ep_num} has already a conflicting device type {existing_ep[DEVICE_TYPE]} and use_type is not set for both."
            )
        return False
    if ep.get(DEVICE_TYPE):
        existing_ep[DEVICE_TYPE] = ep[DEVICE_TYPE]
    existing_ep[CONF_CLUSTERS].extend(ep[CONF_CLUSTERS])
    return True


def create_ep(router: bool) -> None:
    zb_data = CORE.data.setdefault(KEY_ZIGBEE, {})
    ep_dict: dict[int, dict] = zb_data.setdefault(KEY_ZIGBEE_EP, {})
    ep_list: list[dict] = zb_data.setdefault(KEY_ZIGBEE_EP_NO_NUM, [])
    # create dummy endpoint if list is empty
    if not ep_dict and not ep_list:
        ep_type = "CUSTOM_ATTR"
        if router:
            ep_type = "RANGE_EXTENDER"
        ep_dict[1] = {DEVICE_TYPE: ep_type}
    if ep_list:
        # merge endpoint with different clusters
        ep_list_new: list[dict] = []
        for ep in ep_list:
            added = False
            for existing_ep in ep_list_new:
                if merge_endpoint(
                    existing_ep, None, ep, ep.get(CONF_USE_DEVICE_TYPE), True
                ):
                    added = True
                    break
            if not added:
                ep_list_new.append(ep)

        # Add endpoints with no number to the endpoint dict with a new number
        eps = list(ep_dict.keys())
        for ep in ep_list_new:
            ep_num = get_next_ep_num(eps)
            ep_dict[ep_num] = ep

        # clear list so that it is not processed again
        del zb_data[KEY_ZIGBEE_EP_NO_NUM]

    # Add default device type to endpoints that have none
    for ep in ep_dict.values():
        if not ep.get(DEVICE_TYPE):
            ep[DEVICE_TYPE] = "CUSTOM_ATTR"


def add_ep(ep: dict[str, Any], ep_num: int | None, use_type: bool | None) -> None:
    zb_data = CORE.data.setdefault(KEY_ZIGBEE, {})
    if ep_num is None:
        if use_type:
            ep[CONF_USE_DEVICE_TYPE] = use_type
        ep_list: list[dict] = zb_data.setdefault(KEY_ZIGBEE_EP_NO_NUM, [])
        ep_list.append(ep)
    else:
        ep_dict: dict[int, dict] = zb_data.setdefault(KEY_ZIGBEE_EP, {})
        if ep_num in ep_dict:
            # check if the existing endpoint has same clusters
            existing_ep = ep_dict[ep_num]
            merge_endpoint(existing_ep, ep_num, ep, use_type, False)
        else:
            if use_type is not None:
                ep[CONF_USE_DEVICE_TYPE] = use_type
            ep_dict[ep_num] = ep
