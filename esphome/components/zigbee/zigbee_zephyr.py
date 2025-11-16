from datetime import datetime

from esphome import automation
import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf
from esphome.const import CONF_ID, CONF_NAME, __version__
from esphome.core import CORE, ID
from esphome.cpp_generator import (
    AssignmentExpression,
    MockObj,
    VariableDeclarationExpression,
)

from .const_zephyr import (
    CONF_BASIC_ATTRIB_LIST_EXT,
    CONF_BASIC_ATTRS_EXT,
    CONF_BINARY_ATTRS,
    CONF_BINARY_INPUT_ATTRIB_LIST,
    CONF_CLUSTER_LIST,
    CONF_EP,
    CONF_GROUPS_ATTRIB_LIST,
    CONF_GROUPS_ATTRS,
    CONF_IDENTIFY_ATTRIB_LIST,
    CONF_IDENTIFY_ATTRS,
    CONF_ON_JOIN,
    CONF_SCENES_ATTRIB_LIST,
    CONF_SCENES_ATTRS,
    CONF_WIPE_ON_BOOT,
    CONF_ZIGBEE_BINARY_SENSOR,
    CONF_ZIGBEE_ID,
    KEY_EP_NUMBER,
    KEY_ZIGBEE,
    ZB_ZCL_CLUSTER_ID_BASIC,
    ZB_ZCL_CLUSTER_ID_BINARY_INPUT,
    ZB_ZCL_CLUSTER_ID_GROUPS,
    ZB_ZCL_CLUSTER_ID_IDENTIFY,
    ZB_ZCL_CLUSTER_ID_SCENES,
    zb_char_t_ptr,
)


async def zephyr_to_code(config):
    zephyr_add_prj_conf("ZIGBEE", True)
    zephyr_add_prj_conf("ZIGBEE_APP_UTILS", True)
    zephyr_add_prj_conf("ZIGBEE_ROLE_END_DEVICE", True)

    zephyr_add_prj_conf("ZIGBEE_CHANNEL_SELECTION_MODE_MULTI", True)

    zephyr_add_prj_conf("CRYPTO", True)

    zephyr_add_prj_conf("NET_IPV6", False)
    zephyr_add_prj_conf("NET_IP_ADDR_CHECK", False)
    zephyr_add_prj_conf("NET_UDP", False)

    if config[CONF_WIPE_ON_BOOT]:
        cg.add_define("USE_ZIGBEE_WIPE_ON_BOOT")
    var = cg.new_Pvariable(config[CONF_ID])

    if on_join_config := config.get(CONF_ON_JOIN):
        await automation.build_automation(var.get_join_trigger(), [], on_join_config)

    await cg.register_component(var, config)

    await _attr_to_code(config)
    CORE.add_job(_ctx_to_code, config)


async def _attr_to_code(config):
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


def zigbee_register_ep(
    config, cluster_id, report_attr_count: int, clusters, empty_slot: int
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
    CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER][empty_slot] = str(id_)
    obj = cg.RawExpression(
        f"{id_.type}({id_}, {empty_slot + 1}, {cluster_id}, {in_cluster_num}, {out_cluster_num}, {report_attr_count}, {', '.join(attrs)})"
    )
    CORE.add_global(obj)


async def _ctx_to_code(config):
    print("_ctx_to_code")
    cg.add_global(
        cg.RawExpression(
            f"ZBOSS_DECLARE_DEVICE_CTX_EP_VA(zb_device_ctx, &{', &'.join(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])})"
        )
    )
    cg.add(cg.RawExpression("ZB_AF_REGISTER_DEVICE_CTX(&zb_device_ctx)"))


async def zephyr_setup_binary_sensor(entity, config):
    CORE.add_job(_add_binary_sensor, entity, config)


async def _add_binary_sensor(entity, config):
    empty_slot = next(
        (i for i, v in enumerate(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER]) if v == ""), None
    )
    binary_attrs = zigbee_new_variable(config[CONF_BINARY_ATTRS])
    attr_list = zigbee_new_attr_list(
        config[CONF_BINARY_INPUT_ATTRIB_LIST],
        zigbee_assign(binary_attrs.out_of_service, 0),
        zigbee_assign(binary_attrs.present_value, 0),
        zigbee_assign(binary_attrs.status_flags, 0),
        zigbee_set_string(binary_attrs.description, config[CONF_NAME]),
    )

    cluster_id, clusters = zigbee_new_cluster_list(
        config, [ZigbeeClusterDesc(ZB_ZCL_CLUSTER_ID_BINARY_INPUT, attr_list)]
    )

    zigbee_register_ep(config, cluster_id, 2, clusters, empty_slot)

    var = cg.new_Pvariable(config[CONF_ZIGBEE_BINARY_SENSOR], entity)
    await cg.register_component(var, config)

    cg.add(var.set_ep(empty_slot + 1))
    cg.add(var.set_cluster_attributes(binary_attrs))
    hub = await cg.get_variable(config[CONF_ZIGBEE_ID])
    cg.add(var.set_parent(hub))
