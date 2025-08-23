from collections.abc import MutableMapping
from datetime import datetime
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG, Section
from esphome.components.zephyr import zephyr_add_pm_static, zephyr_add_prj_conf
from esphome.components.zephyr.const import KEY_BOOTLOADER, KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.const import CONF_ID, __version__
from esphome.core import CORE, ID, coroutine_with_priority
from esphome.cpp_generator import (
    AssignmentExpression,
    MockObj,
    VariableDeclarationExpression,
)

from .const import (
    CONF_BASIC_ATTRIB_LIST_EXT,
    CONF_BASIC_ATTRS_EXT,
    CONF_CLUSTER_LIST,
    CONF_EP,
    CONF_GROUPS_ATTRIB_LIST,
    CONF_GROUPS_ATTRS,
    CONF_IDENTIFY_ATTRIB_LIST,
    CONF_IDENTIFY_ATTRS,
    CONF_MAX_EP_NUMBER,
    CONF_SCENES_ATTRIB_LIST,
    CONF_SCENES_ATTRS,
    CONF_ZIGBEE_ID,
    ESPHOME_ZB_HA_DECLARE_EP,
    ZB_ZCL_CLUSTER_ID_BASIC,
    ZB_ZCL_CLUSTER_ID_GROUPS,
    ZB_ZCL_CLUSTER_ID_IDENTIFY,
    ZB_ZCL_CLUSTER_ID_SCENES,
    ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT,
    ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST,
    ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST,
    ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST,
    Zigbee,
    zb_char_t_ptr,
    zb_zcl_basic_attrs_ext_t,
    zb_zcl_groups_attrs_t,
    zb_zcl_identify_attrs_t,
    zb_zcl_scenes_attrs_t,
    zigbee_ns,
)

CODEOWNERS = ["@tomaszduda23"]

CONF_ON_JOIN = "on_join"
CONF_WIPE_ON_BOOT = "wipe_on_boot"
KEY_ZIGBEE = "zigbee"
KEY_EP_NUMBER = "ep_number"


def zigbee_set_core_data(config):
    if CORE.data[KEY_ZEPHYR][KEY_BOOTLOADER] in BOOTLOADER_CONFIG:
        zephyr_add_pm_static(
            [Section("empty_after_zboss_offset", 0xF4000, 0xC000, "flash_primary")]
        )

    return config


ZigbeeBaseSchema = cv.Schema(
    {
        cv.GenerateID(CONF_ZIGBEE_ID): cv.use_id(Zigbee),
        cv.GenerateID(CONF_BASIC_ATTRIB_LIST_EXT): cv.use_id(
            ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT
        ),
        cv.GenerateID(CONF_IDENTIFY_ATTRIB_LIST): cv.use_id(
            ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST
        ),
        cv.GenerateID(CONF_GROUPS_ATTRIB_LIST): cv.use_id(
            ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST
        ),
        cv.GenerateID(CONF_SCENES_ATTRIB_LIST): cv.use_id(
            ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST
        ),
        cv.GenerateID(CONF_EP): cv.declare_id(ESPHOME_ZB_HA_DECLARE_EP),
        cv.GenerateID(CONF_CLUSTER_LIST): cv.declare_id(
            cg.global_ns.namespace("zb_zcl_cluster_desc_t")
        ),
    },
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(Zigbee),
            cv.GenerateID(CONF_BASIC_ATTRS_EXT): cv.declare_id(
                zb_zcl_basic_attrs_ext_t
            ),
            cv.GenerateID(CONF_BASIC_ATTRIB_LIST_EXT): cv.declare_id(
                ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT
            ),
            cv.GenerateID(CONF_IDENTIFY_ATTRS): cv.declare_id(zb_zcl_identify_attrs_t),
            cv.GenerateID(CONF_IDENTIFY_ATTRIB_LIST): cv.declare_id(
                ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST
            ),
            cv.GenerateID(CONF_GROUPS_ATTRS): cv.declare_id(zb_zcl_groups_attrs_t),
            cv.GenerateID(CONF_GROUPS_ATTRIB_LIST): cv.declare_id(
                ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST
            ),
            cv.GenerateID(CONF_SCENES_ATTRS): cv.declare_id(zb_zcl_scenes_attrs_t),
            cv.GenerateID(CONF_SCENES_ATTRIB_LIST): cv.declare_id(
                ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST
            ),
            cv.Optional(CONF_ON_JOIN): automation.validate_automation(single=True),
            cv.Optional(CONF_WIPE_ON_BOOT, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    zigbee_set_core_data,
)


def validate_number_of_ep(config):
    count = len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])
    if count > CONF_MAX_EP_NUMBER:
        raise cv.Invalid(f"Maximum number of EP is {CONF_MAX_EP_NUMBER}")
    if count == 0:
        raise cv.Invalid("At least one zigbee device need to be included")


FINAL_VALIDATE_SCHEMA = cv.All(
    validate_number_of_ep,
)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_ZIGBEE")

    zephyr_add_prj_conf("ZIGBEE", True)
    zephyr_add_prj_conf("ZIGBEE_APP_UTILS", True)
    zephyr_add_prj_conf("ZIGBEE_ROLE_END_DEVICE", True)

    zephyr_add_prj_conf("ZIGBEE_CHANNEL_SELECTION_MODE_MULTI", True)

    zephyr_add_prj_conf("CRYPTO", True)

    zephyr_add_prj_conf("NET_IPV6", False)
    zephyr_add_prj_conf("NET_IP_ADDR_CHECK", False)
    zephyr_add_prj_conf("NET_UDP", False)

    basic_attrs_ext = zigbee_new_variable(config[CONF_BASIC_ATTRS_EXT])
    zigbee_new_attr_list(
        config[CONF_BASIC_ATTRIB_LIST_EXT],
        zigbee_assign(
            basic_attrs_ext.zcl_version, cg.global_ns.namespace("ZB_ZCL_VERSION")
        ),
        zigbee_assign(basic_attrs_ext.app_version, 0),
        zigbee_assign(basic_attrs_ext.stack_version, 0),
        zigbee_assign(basic_attrs_ext.hw_version, 0),
        zigbee_set_string(basic_attrs_ext.mf_name, "esphome"),
        zigbee_set_string(basic_attrs_ext.model_id, "v1"),
        zigbee_set_string(
            basic_attrs_ext.date_code, datetime.now().strftime("%d/%m/%y %H:%M")
        ),
        zigbee_assign(
            basic_attrs_ext.power_source,
            cg.global_ns.namespace("ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE"),
        ),
        zigbee_set_string(basic_attrs_ext.location_id, ""),
        zigbee_assign(
            basic_attrs_ext.ph_env,
            cg.global_ns.namespace("ZB_ZCL_BASIC_ENV_UNSPECIFIED"),
        ),
        zigbee_set_string(basic_attrs_ext.sw_ver, __version__),
    )

    identify_attrs = zigbee_new_variable(config[CONF_IDENTIFY_ATTRS])
    zigbee_new_attr_list(
        config[CONF_IDENTIFY_ATTRIB_LIST],
        zigbee_assign(
            identify_attrs.identify_time,
            cg.global_ns.namespace("ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE"),
        ),
    )

    groups_attrs = zigbee_new_variable(config[CONF_GROUPS_ATTRS])
    zigbee_new_attr_list(
        config[CONF_GROUPS_ATTRIB_LIST],
        zigbee_assign(groups_attrs.name_support, 0),
    )

    scenes_attrs = zigbee_new_variable(config[CONF_SCENES_ATTRS])
    zigbee_new_attr_list(
        config[CONF_SCENES_ATTRIB_LIST],
        zigbee_assign(scenes_attrs.scene_count, 0),
        zigbee_assign(scenes_attrs.current_scene, 0),
        zigbee_assign(scenes_attrs.current_group, 0),
        zigbee_assign(scenes_attrs.scene_valid, 0),
        zigbee_assign(scenes_attrs.name_support, 0),
    )

    if CONF_WIPE_ON_BOOT in config:
        cg.add_define("USE_ZIGBEE_WIPE_ON_BOOT")
    var = cg.new_Pvariable(config[CONF_ID])

    if on_join_config := config.get(CONF_ON_JOIN):
        await automation.build_automation(var.get_join_trigger(), [], on_join_config)
    await cg.register_component(var, config)
    CORE.add_job(_ctx_to_code, config)


FactoryResetAction = zigbee_ns.class_("FactoryResetAction", automation.Action)


@automation.register_action("zigbee.factory_reset", FactoryResetAction, cv.Schema({}))
async def zigbee_factory_reset_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


def zigbee_new_variable(id_: ID, type_: "MockObj" = None) -> "MockObj":
    assert isinstance(id_, ID)
    obj = MockObj(id_, ".")
    if type_ is not None:
        id_.type = type_
    decl = VariableDeclarationExpression(id_.type, "", id_)
    CORE.add_global(decl)
    CORE.register_variable(id_, obj)
    return obj


def zigbee_assign(target, expression):
    cg.add(AssignmentExpression("", "", target, expression))
    return target


def zigbee_set_string(target, value: str):
    cg.add(
        cg.RawExpression(
            f"ZB_ZCL_SET_STRING_VAL({target}, {cg.safe_exp(value)}, ZB_ZCL_STRING_CONST_SIZE({cg.safe_exp(value)}))"
        )
    )
    return ID(str(target), True, zb_char_t_ptr)


def zigbee_new_attr_list(id_: ID, *args):
    assert isinstance(id_, ID)
    list = []
    for arg in args:
        if str(zb_char_t_ptr) == str(arg.type) or (
            str(arg) == "zb_zcl_time_attrs_t_id"
        ):
            list.append(f"{arg}")
        else:
            list.append(f"&{arg}")

    obj = cg.RawExpression(f"{id_.type}({id_}, {', '.join(list)})")
    CORE.add_global(obj)
    CORE.register_variable(id_, obj)
    return id_


class ArrayAssignmentExpression(AssignmentExpression):
    __slots__ = ()

    def __init__(self, type_, name, rhs):
        super().__init__(type_, "", name, rhs)

    def __str__(self):
        return f"{self.type} {self.name}[] = {self.rhs}"


def zigbee_array(id_, rhs) -> "MockObj":
    rhs = cg.safe_exp(rhs)
    obj = MockObj(id_, ".")
    assignment = ArrayAssignmentExpression(id_.type, id_, rhs)
    CORE.add_global(assignment)
    CORE.register_variable(id_, obj)
    return obj


class ZigbeeClusterDesc(MockObj):
    def __init__(self, name: str, attr=None):
        self.name = name
        self.attr = attr
        base = ID(name + "_ZHA_", True, type)
        super().__init__(base, "")

    def __str__(self):
        role = (
            "ZB_ZCL_CLUSTER_SERVER_ROLE" if self.attr else "ZB_ZCL_CLUSTER_CLIENT_ROLE"
        )
        attr_count = "0"
        attr_desc_list = "NULL"
        if self.attr:
            attr_count = f"ZB_ZCL_ARRAY_SIZE({self.attr}, zb_zcl_attr_t)"
            attr_desc_list = str(self.attr)
        return f"ZB_ZCL_CLUSTER_DESC({self.name}, {attr_count}, {attr_desc_list}, {role}, ZB_ZCL_MANUF_CODE_INVALID)"


def zigbee_new_cluster_list(config, attr_list):
    rhs = [
        ZigbeeClusterDesc(ZB_ZCL_CLUSTER_ID_BASIC, config[CONF_BASIC_ATTRIB_LIST_EXT]),
        ZigbeeClusterDesc(
            ZB_ZCL_CLUSTER_ID_IDENTIFY, config[CONF_IDENTIFY_ATTRIB_LIST]
        ),
    ]
    rhs.extend([attr_list[0]])
    rhs.extend(
        [
            ZigbeeClusterDesc(
                ZB_ZCL_CLUSTER_ID_GROUPS, config[CONF_GROUPS_ATTRIB_LIST]
            ),
            ZigbeeClusterDesc(
                ZB_ZCL_CLUSTER_ID_SCENES, config[CONF_SCENES_ATTRIB_LIST]
            ),
        ]
    )
    if len(attr_list) == 2:
        rhs.extend([attr_list[1]])
    obj = zigbee_array(config[CONF_CLUSTER_LIST], rhs)
    return (obj, rhs)


def consume_ep_slots(config: MutableMapping) -> MutableMapping:
    data: dict[str, Any] = CORE.data.setdefault(KEY_ZIGBEE, {})
    slots: list[str] = data.setdefault(KEY_EP_NUMBER, [])
    slots.extend([""])
    config[KEY_EP_NUMBER] = len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])
    return config


def zigbee_register_ep(
    config,
    cluster_id,
    report_attr_count: int,
    clusters,
):
    id_ = config[CONF_EP]
    in_cluster_num = 0
    out_cluster_num = 0
    attrs = []
    for c in clusters:
        if c.attr:
            in_cluster_num += 1
        else:
            out_cluster_num += 1
        attrs.append(c.name)
    CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER][config[KEY_EP_NUMBER] - 1] = str(id_)
    obj = cg.RawExpression(
        f"{id_.type}({id_}, {config[KEY_EP_NUMBER]}, {cluster_id}, {in_cluster_num}, {out_cluster_num}, {report_attr_count}, {', '.join(attrs)})"
    )
    CORE.add_global(obj)


@coroutine_with_priority(10.0)
async def _ctx_to_code(config):
    cg.add_global(
        cg.RawExpression(
            f"ZBOSS_DECLARE_DEVICE_CTX_EP_VA(zb_device_ctx, &{', &'.join(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])})"
        )
    )
    cg.add(cg.RawExpression("ZB_AF_REGISTER_DEVICE_CTX(&zb_device_ctx)"))
