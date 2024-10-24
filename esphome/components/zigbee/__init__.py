from datetime import datetime
import random

from esphome import automation
import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf
from esphome.components.zigbee_ctx import KEY_EP, KEY_ZIGBEE, zigbee_set_core_data
import esphome.config_validation as cv
from esphome.const import CONF_BINARY_SENSOR, CONF_ID, CONF_PLATFORM, __version__
from esphome.core import CORE, ID, coroutine_with_priority
from esphome.cpp_generator import (
    AssignmentExpression,
    MockObj,
    VariableDeclarationExpression,
)
import esphome.final_validate as fv

from .const import (
    CONF_BASIC_ATTRIB_LIST_EXT,
    CONF_BASIC_ATTRS_EXT,
    CONF_GROUPS_ATTRIB_LIST,
    CONF_GROUPS_ATTRS,
    CONF_IDENTIFY_ATTRIB_LIST,
    CONF_IDENTIFY_ATTRS,
    CONF_MAX_EP_NUMBER,
    CONF_SCENES_ATTRIB_LIST,
    CONF_SCENES_ATTRS,
    CONF_SWITCH,
    CONF_ZIGBEE_ID,
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

AUTO_LOAD = ["zigbee_ctx"]

CONF_ON_JOIN = "on_join"

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
        }
    ).extend(cv.COMPONENT_SCHEMA),
    zigbee_set_core_data,
)


def count_ep_by_type(fconf, type):
    count = 0
    if type in fconf:
        for entity in fconf[type]:
            if CONF_PLATFORM in entity and entity[CONF_PLATFORM] == KEY_ZIGBEE:
                count += 1
    return count


def validate_number_of_ep(config):
    count = 0
    fconf = fv.full_config.get()
    count += count_ep_by_type(fconf, CONF_SWITCH)
    count += count_ep_by_type(fconf, CONF_BINARY_SENSOR)
    if count > 8:
        raise cv.Invalid(f"Maximum number of EP is {CONF_MAX_EP_NUMBER}")


FINAL_VALIDATE_SCHEMA = cv.All(
    validate_number_of_ep,
)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_ZIGBEE")
    # zigbee
    zephyr_add_prj_conf("ZIGBEE", True)
    zephyr_add_prj_conf("ZIGBEE_APP_UTILS", True)
    zephyr_add_prj_conf("ZIGBEE_ROLE_END_DEVICE", True)

    zephyr_add_prj_conf("ZIGBEE_CHANNEL_SELECTION_MODE_MULTI", True)

    # TODO zigbee2mqtt do not update configuration of device without this
    zephyr_add_prj_conf("IEEE802154_VENDOR_OUI_ENABLE", True)
    random_number = random.randint(0x000000, 0xFFFFFF)
    zephyr_add_prj_conf("IEEE802154_VENDOR_OUI", random_number)

    # crypto
    zephyr_add_prj_conf("CRYPTO", True)

    # networking
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

    # the rest
    var = cg.new_Pvariable(config[CONF_ID])

    if on_join_config := config.get(CONF_ON_JOIN):
        await automation.build_automation(var.get_join_trigger(), [], on_join_config)
    await cg.register_component(var, config)


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
        if str(zb_char_t_ptr) == str(arg.type):
            list.append(f"{arg}")
        else:
            list.append(f"&{arg}")

    obj = cg.RawExpression(f'{id_.type}({id_}, {", ".join(list)})')
    CORE.add_global(obj)
    CORE.register_variable(id_, obj)
    return id_


def zigbee_new_cluster_list(id_: ID, *args):
    assert isinstance(id_, ID)
    list = []
    for arg in args:
        list.append(f"{arg}")
    obj = cg.RawExpression(f'{id_.type}({id_}, {", ".join(list)})')
    CORE.add_global(obj)
    return id_


def zigbee_register_ep(id_: ID, cluster):
    assert isinstance(id_, ID)
    ep = len(CORE.data[KEY_ZIGBEE][KEY_EP]) + 1
    CORE.data[KEY_ZIGBEE][KEY_EP] += [str(id_)]
    obj = cg.RawExpression(f"{id_.type}({id_}, {ep}, {cluster})")
    CORE.add_global(obj)
    return ep
