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


def compare_clusters(
    existing_ep: dict[str, Any],
    ep: dict[str, Any],
) -> tuple[str | int, str] | None:
    existing_clusters = [(cl[CONF_ID], cl[ROLE]) for cl in existing_ep[CONF_CLUSTERS]]
    for cl in [(cl[CONF_ID], cl[ROLE]) for cl in ep[CONF_CLUSTERS]]:
        if cl in existing_clusters:
            return cl
    return None


def merge_endpoints(
    existing_ep: dict[str, Any],
    ep: dict[str, Any],
    use_type: bool | None,
) -> bool:
    if compare_clusters(existing_ep, ep):
        return False
    if (
        ep.get(DEVICE_TYPE)
        and existing_ep.get(DEVICE_TYPE)
        and ep.get(DEVICE_TYPE) != existing_ep.get(DEVICE_TYPE)
    ):
        return False
    if (
        ep.get(DEVICE_TYPE)
        and not existing_ep.get(DEVICE_TYPE)
        and existing_ep.get(CONF_USE_DEVICE_TYPE)
    ):
        return False
    if existing_ep.get(DEVICE_TYPE) and not ep.get(DEVICE_TYPE) and use_type:
        return False
    if use_type:
        existing_ep[CONF_USE_DEVICE_TYPE] = use_type
    if ep.get(DEVICE_TYPE):
        existing_ep[DEVICE_TYPE] = ep[DEVICE_TYPE]
    existing_ep[CONF_CLUSTERS].extend(ep[CONF_CLUSTERS])
    return True


def validate_endpoints(ep_dict: dict[int, dict]) -> None:
    for num, ep in ep_dict.items():
        types_dict = ep.get(CONF_USE_DEVICE_TYPE)
        if not types_dict:
            continue
        if len(types_dict) == 1:
            ep[DEVICE_TYPE] = list(types_dict.keys())[0]
            del ep[CONF_USE_DEVICE_TYPE]
            continue
        types_list = [t[0] for t in types_dict.items() if t[1]]
        if len(types_list) > 1:
            raise cv.Invalid(
                f"There is more than one component with endpoint: {num} and {CONF_USE_DEVICE_TYPE}: True"
            )
        if not types_list:
            raise cv.Invalid(
                f"Multiple device types on endpoint: {num}. Set {CONF_USE_DEVICE_TYPE}: True on one component."
            )
        ep[DEVICE_TYPE] = types_list[0]
        del ep[CONF_USE_DEVICE_TYPE]


def create_ep(router: bool) -> None:
    zb_data = CORE.data.setdefault(KEY_ZIGBEE, {})
    ep_dict: dict[int, dict] = zb_data.setdefault(KEY_ZIGBEE_EP, {})
    ep_list: list[dict] = zb_data.setdefault(KEY_ZIGBEE_EP_NO_NUM, [])
    validate_endpoints(ep_dict)
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
                if merge_endpoints(existing_ep, ep, ep.get(CONF_USE_DEVICE_TYPE)):
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
    if use_type is False:
        ep.pop(DEVICE_TYPE, None)
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
            if cl := compare_clusters(
                existing_ep,
                ep,
            ):
                raise cv.Invalid(
                    f"Endpoint {ep_num} has more than one cluster with cluster id {cl[0]} and role {cl[1]}."
                )
            if ep.get(DEVICE_TYPE) or use_type:
                types_dict = existing_ep.setdefault(CONF_USE_DEVICE_TYPE, {})
                if not types_dict.get(ep.get(DEVICE_TYPE)) or use_type:
                    types_dict[ep.get(DEVICE_TYPE)] = use_type
            existing_ep[CONF_CLUSTERS].extend(ep[CONF_CLUSTERS])
        else:
            if use_type or ep.get(DEVICE_TYPE):
                ep[CONF_USE_DEVICE_TYPE] = {ep.get(DEVICE_TYPE): use_type}
            ep_dict[ep_num] = ep
