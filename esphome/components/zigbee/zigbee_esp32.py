import copy
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
    CONF_ID,
    CONF_MAX_LENGTH,
    CONF_MODEL,
    CONF_NAME,
    CONF_TYPE,
    CONF_VALUE,
    CONF_WIFI,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .const import (
    CONF_REPORT,
    CONF_ROUTER,
    KEY_ZIGBEE,
    REPORT,
    ZIGBEE_DATE,
    ZigbeeAttribute,
)
from .const_esp32 import (
    ATTR_TYPE,
    CLUSTER_ID,
    CONF_ATTRIBUTE_ID,
    CONF_ATTRIBUTES,
    CONF_CLUSTERS,
    CONF_NUM,
    CONF_SCALE,
    DEVICE_ID,
    DEVICE_TYPE,
    KEY_BS_EP,
    ROLE,
)
from .zigbee_ep_esp32 import create_ep, ep_configs


def get_c_size(bits: str, options: list[int]) -> str:
    return str([n for n in options if n >= int(bits)][0])


def get_c_type(attr_type: str) -> Any | None:
    if attr_type == "BOOL":
        return cg.bool_
    if "STRING" in attr_type:
        return cg.std_string
    test = re.match(r"(^U?)(\d{1,2})(BITMAP$|BIT$|BIT_ENUM$|$)", attr_type)
    if test and test.group(2):
        return getattr(cg, "uint" + get_c_size(test.group(2), [8, 16, 32, 64]))
    return None


def get_cv_by_type(attr_type: str) -> Any | None:
    if attr_type == "BOOL":
        return cv.boolean
    if "STRING" in attr_type:
        return cv.string
    test = re.match(r"(^U?)(\d{1,2})(BITMAP$|BIT$|BIT_ENUM$|$)", attr_type)
    if test and test.group(2):
        return cv.positive_int
    return None


def get_default_by_type(attr_type: str) -> str | bool | int:
    if attr_type == "CHAR_STRING":
        return ""
    if attr_type == "BOOL":
        return False
    return 0


def validate_attributes(config: ConfigType) -> ConfigType:
    if CONF_VALUE not in config:
        config[CONF_VALUE] = get_default_by_type(config[CONF_TYPE])
    config[CONF_VALUE] = get_cv_by_type(config[CONF_TYPE])(config[CONF_VALUE])

    return config


def final_validate_esp32(config: ConfigType) -> ConfigType:
    if CONF_WIFI in fv.full_config.get() and CONF_AP in fv.full_config.get()[CONF_WIFI]:
        raise cv.Invalid("Zigbee can't be used together with a Wifi Access Point.")
    if CONF_PARTITIONS in fv.full_config.get() and not isinstance(
        fv.full_config.get()[CONF_PARTITIONS], list
    ):
        with open(
            CORE.relative_config_path(fv.full_config.get()[CONF_PARTITIONS]),
            encoding="utf8",
        ) as f:
            partitions_tab = f.read()
            for partition, types in [
                ("zb_storage", {"type": "data", "subtype": "fat", "size": 0x4000}),
                ("zb_fct", {"type": "data", "subtype": "fat", "size": 0x400}),
            ]:
                if partition not in partitions_tab:
                    cv.Invalid(
                        f"Add '{partition}, {types['type']}, {types['subtype']},   , {types['size']},' to your custom partition table."
                    )
                elif not re.search(
                    rf"^{partition},\s*{types['type']},\s*{types['subtype']}",
                    partitions_tab,
                    re.MULTILINE,
                ):
                    cv.Invalid(
                        f"Partition '{partition}' in your custom partition table has wrong format. It should be: '{partition}, {types['type']}, {types['subtype']},   , {types['size']},'"
                    )
    return config


def validate_binary_sensor_esp32(config: ConfigType) -> ConfigType:
    ep = copy.deepcopy(ep_configs["binary_input"])
    for cl in ep.get(CONF_CLUSTERS, []):
        for attr in cl[CONF_ATTRIBUTES]:
            if (
                attr[CONF_ATTRIBUTE_ID] == 0x1C
                and CONF_VALUE not in attr
                and CONF_NAME in config
            ):  # set name
                name = (
                    config[CONF_NAME].encode("ascii", "ignore").decode()
                )  # use unidecode
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
    zb_data = CORE.data.setdefault(KEY_ZIGBEE, {})
    binary_sensor_ep: list[dict] = zb_data.setdefault(KEY_BS_EP, [])
    binary_sensor_ep.append(ep)
    return config


def zigbee_require_vfs_select(config: ConfigType) -> ConfigType:
    """Register VFS select requirement during config validation."""
    # Zigbee uses esp_vfs_eventfd which requires VFS select support
    if CORE.is_esp32:
        require_vfs_select()
    return config


async def attributes_to_code(
    var: cg.Pvariable, ep_num: int, cl: dict[str, Any]
) -> None:
    for attr in cl.get(CONF_ATTRIBUTES, []):
        if attr.get(CONF_ID) is None:
            cg.add(
                var.add_attr(
                    ep_num,
                    CLUSTER_ID.get(cl[CONF_ID], cl[CONF_ID]),
                    cl[ROLE],
                    attr[CONF_ATTRIBUTE_ID],
                    ATTR_TYPE[attr[CONF_TYPE]],
                    0,
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
            cl[ROLE],
            attr[CONF_ATTRIBUTE_ID],
            ATTR_TYPE[attr[CONF_TYPE]],
            attr.get(CONF_SCALE, 1),
            attr.get(CONF_MAX_LENGTH, 0),
        )
        await cg.register_component(attr_var, attr)

        cg.add(attr_var.add_attr(0, attr[CONF_VALUE]))
        if CONF_REPORT in attr and attr[CONF_REPORT]:
            cg.add(attr_var.set_report(attr[CONF_REPORT] == REPORT["force"]))

        if CONF_DEVICE in attr:
            device = await cg.get_variable(attr[CONF_DEVICE])
            template_arg = cg.TemplateArguments(get_c_type(attr[CONF_TYPE]))
            cg.add(attr_var.connect(template_arg, device))


async def esp32_to_code(config: ConfigType) -> None:
    add_idf_component(
        name="espressif/esp-zboss-lib",
        ref="1.6.4",
    )
    add_idf_component(
        name="espressif/esp-zigbee-lib",
        ref="1.6.8",
    )
    add_idf_sdkconfig_option("CONFIG_ZB_ENABLED", True)
    if config.get(CONF_ROUTER):
        add_idf_sdkconfig_option("CONFIG_ZB_ZCZR", True)
    else:
        add_idf_sdkconfig_option("CONFIG_ZB_ZED", True)
    add_idf_sdkconfig_option("CONFIG_ZB_RADIO_NATIVE", True)
    # The pre-built Zigbee library uses esp_log_default_level which requires
    # dynamic log level control to be enabled
    add_idf_sdkconfig_option("CONFIG_LOG_DYNAMIC_LEVEL_CONTROL", True)
    if CONF_WIFI in CORE.config:
        add_idf_sdkconfig_option("CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE", 4096)
        cg.add_define("CONFIG_WIFI_COEX")

    # add partitions for zigbee
    add_partition("zb_storage", "data", "fat", 0x4000)
    add_partition("zb_fct", "data", "fat", 0x400)

    # create endpoints
    zb_data = CORE.data.get(KEY_ZIGBEE, {})
    binary_sensor_ep: list[dict] = zb_data.get(KEY_BS_EP, [])
    ep_list = create_ep(binary_sensor_ep, config.get(CONF_ROUTER))

    # setup zigbee components
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(
        var.set_basic_cluster(
            config[CONF_MODEL],
            "esphome",
            ZIGBEE_DATE,
        )
    )
    for ep in ep_list:
        cg.add(var.create_default_cluster(ep[CONF_NUM], DEVICE_ID[ep[DEVICE_TYPE]]))
        for cl in ep.get(CONF_CLUSTERS, []):
            cg.add(
                var.add_cluster(
                    ep[CONF_NUM],
                    CLUSTER_ID.get(cl[CONF_ID], cl[CONF_ID]),
                    cl[ROLE],
                )
            )
            await attributes_to_code(var, ep[CONF_NUM], cl)
