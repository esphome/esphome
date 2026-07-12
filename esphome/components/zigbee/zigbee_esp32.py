import copy
import logging
import re
from typing import Any

import esphome.codegen as cg
from esphome.components.esp32 import (
    CONF_PARTITIONS,
    add_idf_component,
    add_idf_sdkconfig_option,
    add_partition,
    require_vfs_select,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_AP,
    CONF_DEVICE,
    CONF_DEVICE_CLASS,
    CONF_ID,
    CONF_MAX_LENGTH,
    CONF_MODEL,
    CONF_NAME,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_VALUE,
    CONF_WIFI,
)
from esphome.core import CORE
from esphome.coroutine import CoroPriority, coroutine_with_priority
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv
from esphome.types import ConfigType

from .const import (
    ANALOG_INPUT_APPTYPE,
    BACNET_UNIT_NO_UNITS,
    BACNET_UNITS,
    CONF_ENDPOINT,
    CONF_POWER_SOURCE,
    CONF_REPORT,
    CONF_ROUTER,
    CONF_USE_DEVICE_TYPE,
    KEY_ZIGBEE,
    POWER_SOURCE,
    ZigbeeAttribute,
)
from .const_esp32 import (
    ATTR_TYPE,
    CLUSTER_ID,
    CLUSTER_ROLE,
    CONF_ATTRIBUTE_ID,
    CONF_ATTRIBUTES,
    CONF_CLUSTERS,
    DEVICE_ID,
    DEVICE_TYPE,
    KEY_ZIGBEE_EP,
    ROLE,
    SCALE,
)
from .zigbee_ep_esp32 import add_ep, create_ep, ep_configs

_LOGGER = logging.getLogger(__name__)


def get_c_size(bits: str, options: list[int]) -> str:
    return str([n for n in options if n >= int(bits)][0])


def get_c_type(attr_type: str) -> Any | None:
    if attr_type == "BOOL":
        return cg.bool_
    if attr_type == "SINGLE":
        return cg.float_
    if attr_type == "DOUBLE":
        return cg.double
    if "STRING" in attr_type:
        return cg.std_string
    test = re.match(r"^(DATA|UINT|MAP|ENUM)(\d{1,2})$", attr_type)
    if test and test.group(2):
        return getattr(cg, "uint" + get_c_size(test.group(2), [8, 16, 32, 64]))
    return None


def get_cv_by_type(attr_type: str) -> Any | None:
    if attr_type == "BOOL":
        return cv.boolean
    if attr_type in ["SINGLE", "DOUBLE"]:
        return cv.float_
    if "STRING" in attr_type:
        return cv.string
    test = re.match(r"^(DATA|UINT|MAP|ENUM)(\d{1,2})$", attr_type)
    if test and test.group(2):
        return cv.positive_int
    raise cv.Invalid(f"Zigbee: type {attr_type} not supported or implemented")


def get_default_by_type(attr_type: str) -> str | bool | int | float:
    if attr_type == "STRING":
        return ""
    if attr_type == "BOOL":
        return False
    if attr_type in ["SINGLE", "DOUBLE"]:
        return float("nan")
    return 0


def validate_attributes(config: ConfigType) -> ConfigType:
    if CONF_VALUE not in config:
        config[CONF_VALUE] = get_default_by_type(config[CONF_TYPE])
    config[CONF_VALUE] = get_cv_by_type(config[CONF_TYPE])(config[CONF_VALUE])

    return config


def final_validate_esp32(config: ConfigType) -> ConfigType:
    if not CORE.is_esp32:
        return config
    if CONF_WIFI in fv.full_config.get():
        if CONF_AP in fv.full_config.get()[CONF_WIFI]:
            raise cv.Invalid(
                "A Wifi Access Point can not be used together with Zigbee."
            )
        if config[CONF_ROUTER]:
            _LOGGER.warning(
                "The Zigbee Router might miss packets while Wifi is active and could destabilize "
                "your network. Use only if Wifi is off most of the time."
            )
    if CONF_PARTITIONS in fv.full_config.get() and not isinstance(
        fv.full_config.get()[CONF_PARTITIONS], list
    ):
        with CORE.relative_config_path(fv.full_config.get()[CONF_PARTITIONS]).open(
            encoding="utf8"
        ) as f:
            partitions_tab = f.read()
            for partition, types in [
                ("zb_fct", {"type": "data", "subtype": "fat", "size": 0x1000}),
            ]:
                if partition not in partitions_tab:
                    raise cv.Invalid(
                        f"Add '{partition}, {types['type']}, {types['subtype']},   , {types['size']},' to your custom partition table."
                    )
                if not re.search(
                    rf"^{partition},\s*{types['type']},\s*{types['subtype']}",
                    partitions_tab,
                    re.MULTILINE,
                ):
                    raise cv.Invalid(
                        f"Partition '{partition}' in your custom partition table has wrong format. It should be: '{partition}, {types['type']}, {types['subtype']},   , {types['size']},'"
                    )
    create_ep(config.get(CONF_ROUTER))
    return config


def setup_attributes(config: ConfigType, clusters: list[dict[str, Any]]) -> None:
    for cl in clusters:
        for attr in cl[CONF_ATTRIBUTES]:
            if (
                attr[CONF_ATTRIBUTE_ID] == 0x1C
                and CONF_VALUE not in attr
                and CONF_NAME in config
            ):  # set name
                name = (
                    config[CONF_NAME].encode("ascii", "ignore").decode()
                )  # or use unidecode
                attr[CONF_VALUE] = str(name)
                attr[CONF_MAX_LENGTH] = len(str(name))
            if CONF_DEVICE in attr:  # connect device
                attr[CONF_DEVICE] = config[CONF_ID]
                if CONF_REPORT in config:
                    attr[CONF_REPORT] = config[CONF_REPORT]
                attr[CONF_ID] = cv.declare_id(ZigbeeAttribute)(None)
                if "zb_attr_ids" not in config:
                    config["zb_attr_ids"] = []
                config["zb_attr_ids"].append(attr[CONF_ID])
            else:
                attr[CONF_ID] = None
            validate_attributes(attr)


def validate_sensor_esp32(config: ConfigType) -> ConfigType:
    ep = copy.deepcopy(ep_configs["analog_input"])
    # get application type from device class and meas unit
    # if none get BACNET unit from meas unit
    dev_class = config.get(CONF_DEVICE_CLASS)
    unit = config.get(CONF_UNIT_OF_MEASUREMENT)
    apptype = ANALOG_INPUT_APPTYPE.get((dev_class, unit))
    bacunit = BACNET_UNITS.get(unit, BACNET_UNIT_NO_UNITS)
    if apptype is not None:
        ep[CONF_CLUSTERS][0][CONF_ATTRIBUTES].append(
            {
                CONF_ATTRIBUTE_ID: 0x100,
                CONF_VALUE: (apptype << 16) | 0xFFFF,
                CONF_TYPE: "UINT32",
            },
        )
    ep[CONF_CLUSTERS][0][CONF_ATTRIBUTES].append(
        {
            CONF_ATTRIBUTE_ID: 0x75,
            CONF_VALUE: bacunit,
            CONF_TYPE: "ENUM16",
        },
    )
    setup_attributes(config, ep[CONF_CLUSTERS])
    add_ep(ep, config.get(CONF_ENDPOINT), config.get(CONF_USE_DEVICE_TYPE))
    return config


def validate_binary_sensor_esp32(config: ConfigType) -> ConfigType:
    ep = copy.deepcopy(ep_configs["binary_input"])
    setup_attributes(config, ep[CONF_CLUSTERS])
    add_ep(ep, config.get(CONF_ENDPOINT), config.get(CONF_USE_DEVICE_TYPE))
    return config


def zigbee_require_vfs_select(config: ConfigType) -> ConfigType:
    """Register VFS select requirement during config validation."""
    # Zigbee uses esp_vfs_eventfd which requires VFS select support
    if CORE.is_esp32:
        require_vfs_select()
    return config


@coroutine_with_priority(CoroPriority.WORKAROUNDS)
async def _zigbee_add_sdkconfigs(config: ConfigType) -> None:
    """Add sdkconfigs late so they can overwrite esp32 defaults"""
    add_idf_sdkconfig_option("CONFIG_ZB_ENABLED", True)
    if config.get(CONF_ROUTER):
        add_idf_sdkconfig_option("CONFIG_ZB_ZCZR", True)
    else:
        add_idf_sdkconfig_option("CONFIG_ZB_ZED", True)
    if CONF_WIFI in CORE.config:
        add_idf_sdkconfig_option("CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE", 4096)


async def attributes_to_code(
    var: cg.Pvariable, ep_num: int, cl: dict[str, Any]
) -> None:
    for attr in cl.get(CONF_ATTRIBUTES, []):
        if attr.get(CONF_ID) is None:
            cg.add(
                var.add_attr(
                    ep_num,
                    CLUSTER_ID.get(cl[CONF_ID], cl[CONF_ID]),
                    CLUSTER_ROLE[cl[ROLE]],
                    attr[CONF_ATTRIBUTE_ID],
                    attr.get(CONF_MAX_LENGTH, 0),
                    attr[CONF_VALUE],
                )
            )
            continue
        attr_var = cg.new_Pvariable(
            attr[CONF_ID],
            var,
            ep_num,
            CLUSTER_ID.get(cl[CONF_ID], cl[CONF_ID]),
            CLUSTER_ROLE[cl[ROLE]],
            attr[CONF_ATTRIBUTE_ID],
            ATTR_TYPE[attr[CONF_TYPE]],
            attr.get(SCALE, 1),
            attr.get(CONF_MAX_LENGTH, 0),
        )
        await cg.register_component(attr_var, attr)

        cg.add(attr_var.add_attr(attr[CONF_VALUE]))
        if CONF_REPORT in attr:
            cg.add(attr_var.set_report(attr[CONF_REPORT]))

        if CONF_DEVICE in attr:
            device = await cg.get_variable(attr[CONF_DEVICE])
            template_arg = cg.TemplateArguments(get_c_type(attr[CONF_TYPE]))
            cg.add(attr_var.connect(template_arg, device))


async def esp32_to_code(config: ConfigType) -> "MockObj":
    add_idf_component(
        name="espressif/esp-zigbee-lib",
        ref="2.0.2",
    )

    # add sdkconfigs later so they can overwrite esp32 defaults
    CORE.add_job(_zigbee_add_sdkconfigs, config)

    # add partitions for zigbee
    add_partition("zb_fct", "data", "fat", 0x1000)  # 4KB, minimum size

    # create endpoints
    zb_data = CORE.data.get(KEY_ZIGBEE, {})
    ep_dict: dict[int, dict] = zb_data.get(KEY_ZIGBEE_EP, {})

    # setup zigbee components
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(
        var.set_basic_cluster(
            config[CONF_MODEL],
            "esphome",
            POWER_SOURCE[config[CONF_POWER_SOURCE]],
        )
    )
    for ep_num, ep in ep_dict.items():
        cg.add(var.create_default_cluster(ep_num, DEVICE_ID[ep[DEVICE_TYPE]]))
        for cl in ep.get(CONF_CLUSTERS, []):
            cg.add(
                var.add_cluster(
                    ep_num,
                    CLUSTER_ID.get(cl[CONF_ID], cl[CONF_ID]),
                    CLUSTER_ROLE[cl[ROLE]],
                )
            )
            await attributes_to_code(var, ep_num, cl)
    return var
