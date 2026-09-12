from typing import Any

import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_TYPE,
    CONF_VALUE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_OCCUPANCY,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    UNIT_CELSIUS,
    UNIT_CUBIC_METER_PER_HOUR,
    UNIT_HECTOPASCAL,
    UNIT_LITRE_PER_HOUR,
    UNIT_LUX,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PARTS_PER_MILLION,
    UNIT_PASCAL,
    UNIT_PERCENT,
)
from esphome.core import CORE, Lambda

from .const import (
    CONF_MAX_EP_NUMBER,
    CONF_REPORT,
    CONF_USE_DEVICE_TYPE,
    KEY_ZIGBEE,
    REPORT,
)
from .const_esp32 import (
    ALLOWED_UNITS,
    CONF_ATTRIBUTE_ID,
    CONF_ATTRIBUTES,
    CONF_CLUSTERS,
    CONNECT,
    DEVICE_TYPE,
    KEY_ZIGBEE_EP,
    KEY_ZIGBEE_EP_NO_NUM,
    KEY_ZIGBEE_FIRST_EP_CL,
    ROLE,
    SCALE,
)

# endpoint configs:
ANALOG_INPUT_EP = {
    CONF_CLUSTERS: [
        {
            CONF_ID: "ANALOG_INPUT",
            ROLE: "SERVER",
            CONF_ATTRIBUTES: [
                {
                    CONF_ATTRIBUTE_ID: 0x55,
                    CONF_TYPE: "SINGLE",
                    CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                    CONNECT: True,
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
}

BINARY_INPUT_EP = {
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
                    CONNECT: True,
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
}

BINARY_OUTPUT_EP = {
    CONF_CLUSTERS: [
        {
            CONF_ID: "BINARY_OUTPUT",
            ROLE: "SERVER",
            CONF_ATTRIBUTES: [
                {
                    CONF_ATTRIBUTE_ID: 0x55,
                    CONF_TYPE: "BOOL",
                    CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                    CONNECT: True,
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
}


def _pressure_ep(device_type: bool = False) -> dict[str, Any]:
    ep = {
        ALLOWED_UNITS: [UNIT_HECTOPASCAL, UNIT_PASCAL],
        CONF_CLUSTERS: [
            {
                CONF_ID: "PRESSURE_MEASUREMENT",
                ROLE: "SERVER",
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "INT16",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONNECT: True,
                        SCALE: {
                            UNIT_HECTOPASCAL: 1,
                            UNIT_PASCAL: 0.01,
                        },
                    },
                ],
            },
        ],
    }
    if device_type:
        ep[DEVICE_TYPE] = (
            "PRESSURE_SENSOR"  # Sensor that measures pressure of liquids like water
        )
    return ep


SENSOR_EP_CONFIGS: dict[str, dict[str, Any]] = {
    DEVICE_CLASS_TEMPERATURE: {
        ALLOWED_UNITS: [UNIT_CELSIUS],
        DEVICE_TYPE: "TEMPERATURE_SENSOR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "TEMPERATURE_MEASUREMENT",
                ROLE: "SERVER",
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "INT16",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        SCALE: 100,
                        CONNECT: True,
                    },
                ],
            },
        ],
    },
    DEVICE_CLASS_HUMIDITY: {
        ALLOWED_UNITS: [UNIT_PERCENT],
        CONF_CLUSTERS: [
            {
                CONF_ID: "REL_HUMIDITY_MEASUREMENT",
                ROLE: "SERVER",
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "UINT16",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        SCALE: 100,
                        CONNECT: True,
                    },
                ],
            },
        ],
    },
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE: _pressure_ep(),
    DEVICE_CLASS_PRESSURE: _pressure_ep(device_type=True),
    DEVICE_CLASS_VOLUME_FLOW_RATE: {
        ALLOWED_UNITS: [UNIT_LITRE_PER_HOUR, UNIT_CUBIC_METER_PER_HOUR],
        DEVICE_TYPE: "FLOW_SENSOR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "FLOW_MEASUREMENT",
                ROLE: "SERVER",
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "UINT16",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONNECT: True,
                        SCALE: {
                            UNIT_LITRE_PER_HOUR: 0.01,
                            UNIT_CUBIC_METER_PER_HOUR: 10,
                        },
                    },
                ],
            },
        ],
    },
    DEVICE_CLASS_ILLUMINANCE: {
        ALLOWED_UNITS: [UNIT_LUX],
        DEVICE_TYPE: "LIGHT_SENSOR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "ILLUMINANCE_MEASUREMENT",
                ROLE: "SERVER",
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "UINT16",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONF_LAMBDA: cv.lambda_(
                            Lambda(
                                "if (x < 0.0f || std::isnan(x)) return 0xFFFF;"  # NaN
                                " if (x < 1.0f) return 0;"  # too small to measure
                                " const float v = log10(x)*10000 + 1;"
                                " return v > 65534.0f ? 0xFFFE : (uint16_t) lroundf(v);"  # clamp to 0xFFFE if too large
                            )
                        ),
                        CONNECT: True,
                    },
                ],
            },
        ],
    },
    DEVICE_CLASS_PM25: {
        ALLOWED_UNITS: [UNIT_MICROGRAMS_PER_CUBIC_METER],
        CONF_CLUSTERS: [
            {
                CONF_ID: "PM2_5_MEASUREMENT",
                ROLE: "SERVER",
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "SINGLE",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONNECT: True,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x2,
                        CONF_TYPE: "SINGLE",
                        CONF_VALUE: 9999,  # overwrite default 1.0
                    },
                ],
            },
        ],
    },
    DEVICE_CLASS_CARBON_DIOXIDE: {
        ALLOWED_UNITS: [UNIT_PARTS_PER_MILLION],
        CONF_CLUSTERS: [
            {
                CONF_ID: "CARBON_DIOXIDE_MEASUREMENT",
                ROLE: "SERVER",
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0000,
                        CONF_TYPE: "SINGLE",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONNECT: True,
                        SCALE: 0.000001,
                    },
                    {CONF_ATTRIBUTE_ID: 0x0001, CONF_TYPE: "SINGLE", CONF_VALUE: 0.0},
                    {CONF_ATTRIBUTE_ID: 0x0002, CONF_TYPE: "SINGLE", CONF_VALUE: 0.1},
                ],
            },
        ],
    },
}

BINARY_SENSOR_EP_CONFIGS: dict[str, dict[str, Any]] = {
    DEVICE_CLASS_OCCUPANCY: {
        DEVICE_TYPE: "OCCUPANCY_SENSOR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "OCCUPANCY_SENSING",
                ROLE: "SERVER",
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "MAP8",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONNECT: True,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x1,
                        CONF_TYPE: "ENUM8",
                        CONF_VALUE: 0,  # hardcode PIR for now as ultrasonic or physical contact is unlikely
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x2,
                        CONF_TYPE: "MAP8",
                        CONF_VALUE: 0b00000001,  # hardcode PIR for now as ultrasonic or physical contact is unlikely
                    },
                ],
            },
        ],
    },
}

SWITCH_EP_CONFIGS: dict[str, dict[str, Any]] = {
    "on_off": {
        DEVICE_TYPE: "ON_OFF_OUTPUT",
        CONF_CLUSTERS: [
            {
                CONF_ID: "ON_OFF",
                ROLE: "SERVER",
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: cv.enum(REPORT, lower=True)("default"),
                        CONNECT: True,
                    },
                ],
            },
        ],
    },
}


def _get_next_ep_num(eps: list[int]) -> int:
    try:
        ep_num = [i for i in range(1, CONF_MAX_EP_NUMBER + 1) if i not in eps][0]
        eps.append(ep_num)
    except IndexError as e:
        raise cv.Invalid(
            f"Too many devices. Zigbee can define only {CONF_MAX_EP_NUMBER} endpoints."
        ) from e
    return ep_num


def _compare_clusters(
    existing_cl_list: list[dict[str, Any]],
    cl_list: list[dict[str, Any]],
) -> tuple[str | int, str] | None:
    existing_clusters = [(cl[CONF_ID], cl[ROLE]) for cl in existing_cl_list]
    for cl in [(cl[CONF_ID], cl[ROLE]) for cl in cl_list]:
        if cl in existing_clusters:
            return cl
    return None


def _merge_endpoints(
    existing_ep: dict[str, Any],
    ep: dict[str, Any],
    use_type: bool | None,
) -> bool:
    if _compare_clusters(existing_ep.get(CONF_CLUSTERS, []), ep.get(CONF_CLUSTERS, [])):
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


def _validate_endpoints(ep_dict: dict[int, dict]) -> None:
    """Validate endpoint device type selection before endpoint creation.

    This resolves any deferred device type selections stored in CONF_USE_DEVICE_TYPE,
    ensuring each endpoint has at most one active device type.
    """
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
    """Finalize Zigbee endpoint creation and normalize endpoint storage.

    Validate endpoints, merge endpoints, and assign numbers to endpoints without an explicit number.
    This is called from final_validate.

    Args:
        router: Whether the device is acting as a Zigbee router.
    """
    zb_data = CORE.data.setdefault(KEY_ZIGBEE, {})
    ep_dict: dict[int, dict] = zb_data.setdefault(KEY_ZIGBEE_EP, {})
    ep_list: list[dict] = zb_data.setdefault(KEY_ZIGBEE_EP_NO_NUM, [])
    _validate_endpoints(ep_dict)
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
                if _merge_endpoints(existing_ep, ep, ep.get(CONF_USE_DEVICE_TYPE)):
                    added = True
                    break
            if not added:
                ep_list_new.append(ep)

        # Add endpoints with no number to the endpoint dict with a new number
        eps = list(ep_dict.keys())
        for ep in ep_list_new:
            ep_num = _get_next_ep_num(eps)
            ep_dict[ep_num] = ep

        # clear list so that it is not processed again
        del zb_data[KEY_ZIGBEE_EP_NO_NUM]
    # Add clusters to first ep
    cl_list: list[dict] = zb_data.setdefault(KEY_ZIGBEE_FIRST_EP_CL, [])
    if cl_list:
        first_ep = ep_dict[get_first_ep_num()]
        first_ep.setdefault(CONF_CLUSTERS, [])
        if cl := _compare_clusters(first_ep[CONF_CLUSTERS], cl_list):
            raise cv.Invalid(
                f"Endpoint {get_first_ep_num()} has more than one cluster with cluster id {cl[0]} and role {cl[1]}."
            )
        first_ep[CONF_CLUSTERS] += cl_list
        del zb_data[KEY_ZIGBEE_FIRST_EP_CL]

    # Add default device type to endpoints that have none
    for ep in ep_dict.values():
        if not ep.get(DEVICE_TYPE):
            ep[DEVICE_TYPE] = "CUSTOM_ATTR"


def get_first_ep_num() -> int | None:
    """Return the number of the first endpoint."""
    zb_data = CORE.data.setdefault(KEY_ZIGBEE, {})
    ep_dict: dict[int, dict] = zb_data.setdefault(KEY_ZIGBEE_EP, {})
    if ep_dict:
        return min(ep_dict.keys())
    return None


def add_ep(ep: dict[str, Any], ep_num: int | None, use_type: bool | None) -> None:
    """Add a Zigbee endpoint configuration to CORE.data.

    Args:
        ep: Endpoint configuration dictionary.
        ep_num: Optional explicit endpoint number.
        use_type: Optional boolean indicating whether this component's device type should be
        used for the endpoint (True claims it, False drops it, None leaves it as a candidate).
    """
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
            if cl := _compare_clusters(
                existing_ep.get(CONF_CLUSTERS, []),
                ep.get(CONF_CLUSTERS, []),
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


def add_clusters_to_first_ep(cl: list[dict[str, Any]]) -> None:
    """Add a list of Zigbee clusters to CORE.data.

    Args:
        cl: list of cluster dictonaries.
    """
    zb_data = CORE.data.setdefault(KEY_ZIGBEE, {})
    cl_list: list[dict] = zb_data.setdefault(KEY_ZIGBEE_FIRST_EP_CL, [])
    if cluster := _compare_clusters(
        cl_list,
        cl,
    ):
        raise cv.Invalid(
            f"Only one cluster with cluster id {cluster[0]} and role {cluster[1]} can be added to first endpoint."
        )
    cl_list += cl
