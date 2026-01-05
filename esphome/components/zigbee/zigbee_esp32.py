import copy
import datetime
import re

from esphome import automation
import esphome.codegen as cg
from esphome.components.esp32 import (
    add_idf_component,
    add_idf_sdkconfig_option,
    only_on_variant,
    require_vfs_select,
    validate_custom_partition,
)
from esphome.components.esp32.const import VARIANT_ESP32C6, VARIANT_ESP32H2
import esphome.config_validation as cv
from esphome.const import (
    CONF_AP,
    CONF_DEVICE,
    CONF_ID,
    CONF_INTERNAL,
    CONF_MAX_LENGTH,
    CONF_NAME,
    CONF_TYPE,
    CONF_VALUE,
    CONF_WIFI,
)
from esphome.core import CORE
import esphome.final_validate as fv

from .const import (
    CONF_ATTRIBUTE_ID,
    CONF_ATTRIBUTES,
    CONF_CLUSTERS,
    CONF_NUM,
    CONF_REPORT,
    CONF_ROUTER,
    CONF_SCALE,
    DEVICE_TYPE,
    KEY_BS_EP,
    KEY_ZIGBEE,
    REPORT,
    ROLE,
    FactoryResetAction,
    ZigbeeAttribute,
    ZigbeeComponent,
)
from .const_esp32 import ATTR_TYPE, CLUSTER_ID, CLUSTER_ROLE, DEVICE_ID
from .zigbee_ep_esp32 import create_ep, ep_configs


def get_c_size(bits, options):
    return str([n for n in options if n >= int(bits)][0])


def get_c_type(attr_type):
    if attr_type == "BOOL":
        return cg.bool_
    if "STRING" in attr_type:
        return cg.std_string
    test = re.match(r"(^U?)(\d{1,2})(BITMAP$|BIT$|BIT_ENUM$|$)", attr_type)
    if test and test.group(2):
        return getattr(cg, "uint" + get_c_size(test.group(2), [8, 16, 32, 64]))
    return None


def get_cv_by_type(attr_type):
    if attr_type == "BOOL":
        return cv.boolean
    if "STRING" in attr_type:
        return cv.string
    test = re.match(r"(^U?)(\d{1,2})(BITMAP$|BIT$|BIT_ENUM$|$)", attr_type)
    if test and test.group(2):
        return cv.positive_int
    return None


def get_default_by_type(attr_type):
    if attr_type == "CHAR_STRING":
        return ""
    if attr_type == "BOOL":
        return False
    return 0


def validate_attributes(config):
    if CONF_VALUE not in config:
        config[CONF_VALUE] = get_default_by_type(config[CONF_TYPE])
    config[CONF_VALUE] = get_cv_by_type(config[CONF_TYPE])(config[CONF_VALUE])

    return config


def final_validate(config):
    if CONF_WIFI in fv.full_config.get() and CONF_AP in fv.full_config.get()[CONF_WIFI]:
        raise cv.Invalid("Zigbee can't be used together with a Wifi Access Point.")
    return config


FINAL_VALIDATE_SCHEMA = cv.Schema(final_validate)


def validate_binary_sensor(config):
    if "zigbee" not in CORE.loaded_integrations:
        return config
    if (CONF_NAME in config) and not config.get(CONF_INTERNAL):
        ep = copy.deepcopy(ep_configs["binary_input"])
        for cl in ep.get(CONF_CLUSTERS, []):
            for attr in cl[CONF_ATTRIBUTES]:
                if CONF_DEVICE in attr:  # connect device
                    attr[CONF_DEVICE] = config[CONF_ID]
                    if CONF_REPORT in config:
                        attr[CONF_REPORT] = config[CONF_REPORT]
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
                validate_attributes(attr)
                attr[CONF_ID] = cv.declare_id(ZigbeeAttribute)(None)
                if "zb_attr_ids" not in config:
                    config["zb_attr_ids"] = []
                config["zb_attr_ids"].append(attr[CONF_ID])
        zb_data = CORE.data.setdefault(KEY_ZIGBEE, {})
        binary_sensor_ep: list[dict] = zb_data.setdefault(KEY_BS_EP, [])
        binary_sensor_ep.append(ep)
    return config


BINARY_SENSOR_SCHEMA = {
    cv.Optional(CONF_REPORT): cv.All(
        cv.requires_component("zigbee"),
        cv.enum(REPORT, lower=True),
    )
}


def _require_vfs_select(config):
    """Register VFS select requirement during config validation."""
    # Zigbee uses esp_vfs_eventfd which requires VFS select support
    require_vfs_select()
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZigbeeComponent),
            cv.Optional(CONF_NAME): cv.string,
            cv.Optional(CONF_ROUTER, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_custom_partition("zb_storage", "data", "fat", 0x4000),
    validate_custom_partition("zb_fct", "data", "fat", 0x400),
    cv.require_framework_version(esp_idf=cv.Version(5, 1, 2)),
    only_on_variant(
        supported=[
            VARIANT_ESP32H2,
            VARIANT_ESP32C6,
        ]
    ),
    _require_vfs_select,
)


async def attributes_to_code(var, ep_num, cl):
    for attr in cl.get(CONF_ATTRIBUTES, []):
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


async def to_code(config):
    cg.add_define("USE_ZIGBEE")
    add_idf_component(
        name="espressif/esp-zboss-lib",
        ref="1.6.4",
    )
    add_idf_component(
        name="espressif/esp-zigbee-lib",
        ref="1.6.7",
    )
    add_idf_sdkconfig_option("CONFIG_ZB_ENABLED", True)
    if config.get(CONF_ROUTER):
        add_idf_sdkconfig_option("CONFIG_ZB_ZCZR", True)
    else:
        add_idf_sdkconfig_option("CONFIG_ZB_ZED", True)
    add_idf_sdkconfig_option("CONFIG_ZB_RADIO_NATIVE", True)
    if CONF_WIFI in CORE.config:
        add_idf_sdkconfig_option("CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE", 4096)
        cg.add_define("CONFIG_WIFI_COEX")

    # create endpoints
    zb_data = CORE.data.get(KEY_ZIGBEE, {})
    binary_sensor_ep: list[dict] = zb_data.get(KEY_BS_EP, [])
    ep_list = create_ep(binary_sensor_ep)

    # setup zigbee components
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_NAME not in config:
        config[CONF_NAME] = CORE.name or ""
    cg.add(
        var.set_basic_cluster(
            config[CONF_NAME],
            "esphome",
            datetime.datetime.now().strftime("%Y%m%d"),
        )
    )
    for ep in ep_list:
        cg.add(var.create_default_cluster(ep[CONF_NUM], DEVICE_ID[ep[DEVICE_TYPE]]))
        cg.add(
            var.add_cluster(ep[CONF_NUM], CLUSTER_ID["BASIC"], CLUSTER_ROLE["SERVER"])
        )
        for cl in ep.get(CONF_CLUSTERS, []):
            cg.add(
                var.add_cluster(
                    ep[CONF_NUM],
                    CLUSTER_ID.get(cl[CONF_ID], cl[CONF_ID]),
                    cl[ROLE],
                )
            )
            await attributes_to_code(var, ep[CONF_NUM], cl)


ZIGBEE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ZigbeeComponent),
    }
)


@automation.register_action(
    "zigbee.factory_reset",
    FactoryResetAction,
    automation.maybe_simple_id(ZIGBEE_ACTION_SCHEMA),
)
async def reset_zigbee_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
