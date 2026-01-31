from datetime import datetime
import random

from esphome import automation
import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_UNIT_OF_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_CENTIMETER,
    UNIT_DECIBEL,
    UNIT_HECTOPASCAL,
    UNIT_HERTZ,
    UNIT_HOUR,
    UNIT_KELVIN,
    UNIT_KILOMETER,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_LUX,
    UNIT_METER,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_MILLIAMP,
    UNIT_MILLIGRAMS_PER_CUBIC_METER,
    UNIT_MILLIMETER,
    UNIT_MILLISECOND,
    UNIT_MILLIVOLT,
    UNIT_MINUTE,
    UNIT_OHM,
    UNIT_PARTS_PER_BILLION,
    UNIT_PARTS_PER_MILLION,
    UNIT_PASCAL,
    UNIT_PERCENT,
    UNIT_SECOND,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
    __version__,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.cpp_generator import (
    AssignmentExpression,
    MockObj,
    VariableDeclarationExpression,
)
from esphome.types import ConfigType

from .const_zephyr import (
    CONF_ON_JOIN,
    CONF_POWER_SOURCE,
    CONF_WIPE_ON_BOOT,
    CONF_ZIGBEE_BINARY_SENSOR,
    CONF_ZIGBEE_ID,
    CONF_ZIGBEE_SENSOR,
    CONF_ZIGBEE_SWITCH,
    KEY_EP_NUMBER,
    KEY_ZIGBEE,
    POWER_SOURCE,
    ZB_ZCL_BASIC_ATTRS_EXT_T,
    ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
    ZB_ZCL_CLUSTER_ID_BASIC,
    ZB_ZCL_CLUSTER_ID_BINARY_INPUT,
    ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT,
    ZB_ZCL_CLUSTER_ID_IDENTIFY,
    ZB_ZCL_IDENTIFY_ATTRS_T,
    AnalogAttrs,
    BinaryAttrs,
    ZigbeeComponent,
    zigbee_ns,
)

ZigbeeBinarySensor = zigbee_ns.class_("ZigbeeBinarySensor", cg.Component)
ZigbeeSensor = zigbee_ns.class_("ZigbeeSensor", cg.Component)
ZigbeeSwitch = zigbee_ns.class_("ZigbeeSwitch", cg.Component)

# BACnet engineering units mapping (ZCL uses BACnet unit codes)
# See: https://github.com/zigpy/zha/blob/dev/zha/application/platforms/number/bacnet.py
BACNET_UNITS = {
    UNIT_CELSIUS: 62,
    UNIT_KELVIN: 63,
    UNIT_VOLT: 5,
    UNIT_MILLIVOLT: 124,
    UNIT_AMPERE: 3,
    UNIT_MILLIAMP: 2,
    UNIT_OHM: 4,
    UNIT_WATT: 47,
    UNIT_KILOWATT: 48,
    UNIT_WATT_HOURS: 18,
    UNIT_KILOWATT_HOURS: 19,
    UNIT_PASCAL: 53,
    UNIT_HECTOPASCAL: 133,
    UNIT_HERTZ: 27,
    UNIT_MILLIMETER: 30,
    UNIT_CENTIMETER: 118,
    UNIT_METER: 31,
    UNIT_KILOMETER: 193,
    UNIT_MILLISECOND: 159,
    UNIT_SECOND: 73,
    UNIT_MINUTE: 72,
    UNIT_HOUR: 71,
    UNIT_PARTS_PER_MILLION: 96,
    UNIT_PARTS_PER_BILLION: 97,
    UNIT_MICROGRAMS_PER_CUBIC_METER: 219,
    UNIT_MILLIGRAMS_PER_CUBIC_METER: 218,
    UNIT_LUX: 37,
    UNIT_DECIBEL: 199,
    UNIT_PERCENT: 98,
}
BACNET_UNIT_NO_UNITS = 95

zephyr_binary_sensor = cv.Schema(
    {
        cv.OnlyWith(CONF_ZIGBEE_ID, ["nrf52", "zigbee"]): cv.use_id(ZigbeeComponent),
        cv.OnlyWith(CONF_ZIGBEE_BINARY_SENSOR, ["nrf52", "zigbee"]): cv.declare_id(
            ZigbeeBinarySensor
        ),
    }
)

zephyr_sensor = cv.Schema(
    {
        cv.OnlyWith(CONF_ZIGBEE_ID, ["nrf52", "zigbee"]): cv.use_id(ZigbeeComponent),
        cv.OnlyWith(CONF_ZIGBEE_SENSOR, ["nrf52", "zigbee"]): cv.declare_id(
            ZigbeeSensor
        ),
    }
)

zephyr_switch = cv.Schema(
    {
        cv.OnlyWith(CONF_ZIGBEE_ID, ["nrf52", "zigbee"]): cv.use_id(ZigbeeComponent),
        cv.OnlyWith(CONF_ZIGBEE_SWITCH, ["nrf52", "zigbee"]): cv.declare_id(
            ZigbeeSwitch
        ),
    }
)


async def zephyr_to_code(config: ConfigType) -> None:
    zephyr_add_prj_conf("ZIGBEE", True)
    zephyr_add_prj_conf("ZIGBEE_APP_UTILS", True)
    zephyr_add_prj_conf("ZIGBEE_ROLE_END_DEVICE", True)

    zephyr_add_prj_conf("ZIGBEE_CHANNEL_SELECTION_MODE_MULTI", True)

    zephyr_add_prj_conf("CRYPTO", True)

    zephyr_add_prj_conf("NET_IPV6", False)
    zephyr_add_prj_conf("NET_IP_ADDR_CHECK", False)
    zephyr_add_prj_conf("NET_UDP", False)

    if config[CONF_WIPE_ON_BOOT]:
        if config[CONF_WIPE_ON_BOOT] == "once":
            cg.add_define(
                "USE_ZIGBEE_WIPE_ON_BOOT_MAGIC", random.randint(0x000001, 0xFFFFFF)
            )
        cg.add_define("USE_ZIGBEE_WIPE_ON_BOOT")
    var = cg.new_Pvariable(config[CONF_ID])

    if on_join_config := config.get(CONF_ON_JOIN):
        await automation.build_automation(var.get_join_trigger(), [], on_join_config)

    await cg.register_component(var, config)

    await _attr_to_code(config)
    CORE.add_job(_ctx_to_code, config)


async def _attr_to_code(config: ConfigType) -> None:
    # Create the basic attributes structure and attribute list
    basic_attrs = zigbee_new_variable("zigbee_basic_attrs", ZB_ZCL_BASIC_ATTRS_EXT_T)
    zigbee_new_attr_list(
        "zigbee_basic_attrib_list",
        "ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT",
        zigbee_assign(basic_attrs.zcl_version, cg.RawExpression("ZB_ZCL_VERSION")),
        zigbee_assign(basic_attrs.app_version, 0),
        zigbee_assign(basic_attrs.stack_version, 0),
        zigbee_assign(basic_attrs.hw_version, 0),
        zigbee_set_string(basic_attrs.mf_name, "esphome"),
        zigbee_set_string(basic_attrs.model_id, CORE.name),
        zigbee_set_string(
            basic_attrs.date_code, datetime.now().strftime("%d/%m/%y %H:%M")
        ),
        zigbee_assign(
            basic_attrs.power_source,
            cg.RawExpression(POWER_SOURCE[config[CONF_POWER_SOURCE]]),
        ),
        zigbee_set_string(basic_attrs.location_id, ""),
        zigbee_assign(
            basic_attrs.ph_env, cg.RawExpression("ZB_ZCL_BASIC_ENV_UNSPECIFIED")
        ),
        zigbee_set_string(basic_attrs.sw_ver, __version__),
    )

    # Create the identify attributes structure and attribute list
    identify_attrs = zigbee_new_variable(
        "zigbee_identify_attrs", ZB_ZCL_IDENTIFY_ATTRS_T
    )
    zigbee_new_attr_list(
        "zigbee_identify_attrib_list",
        "ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST",
        zigbee_assign(
            identify_attrs.identify_time,
            cg.RawExpression("ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE"),
        ),
    )


def zigbee_new_variable(name: str, type_: str) -> cg.MockObj:
    """Create a global variable with the given name and type."""
    decl = VariableDeclarationExpression(type_, "", name)
    CORE.add_global(decl)
    return MockObj(name, ".")


def zigbee_assign(target: cg.MockObj, expression: cg.RawExpression | int) -> str:
    """Assign an expression to a target and return a reference to it."""
    cg.add(AssignmentExpression("", "", target, expression))
    return f"&{target}"


def zigbee_set_string(target: cg.MockObj, value: str) -> str:
    """Set a ZCL string value and return the target name (arrays decay to pointers)."""
    # Zigbee supports only ASCII
    value = value.encode("ascii", "ignore").decode()
    cg.add(
        cg.RawExpression(
            f"ZB_ZCL_SET_STRING_VAL({target}, {cg.safe_exp(value)}, ZB_ZCL_STRING_CONST_SIZE({cg.safe_exp(value)}))"
        )
    )
    return str(target)


def zigbee_new_attr_list(name: str, macro: str, *args: str) -> str:
    """Create an attribute list using a ZBOSS macro and return the name."""
    obj = cg.RawExpression(f"{macro}({name}, {', '.join(args)})")
    CORE.add_global(obj)
    return name


class ZigbeeClusterDesc:
    """Represents a Zigbee cluster descriptor for code generation."""

    def __init__(self, cluster_id: str, attr_list_name: str | None = None) -> None:
        self._cluster_id = cluster_id
        self._attr_list_name = attr_list_name

    @property
    def cluster_id(self) -> str:
        return self._cluster_id

    @property
    def has_attrs(self) -> bool:
        return self._attr_list_name is not None

    def __str__(self) -> str:
        role = (
            "ZB_ZCL_CLUSTER_SERVER_ROLE"
            if self._attr_list_name
            else "ZB_ZCL_CLUSTER_CLIENT_ROLE"
        )
        if self._attr_list_name:
            attr_count = f"ZB_ZCL_ARRAY_SIZE({self._attr_list_name}, zb_zcl_attr_t)"
            return f"ZB_ZCL_CLUSTER_DESC({self._cluster_id}, {attr_count}, {self._attr_list_name}, {role}, ZB_ZCL_MANUF_CODE_INVALID)"
        return f"ZB_ZCL_CLUSTER_DESC({self._cluster_id}, 0, NULL, {role}, ZB_ZCL_MANUF_CODE_INVALID)"


def zigbee_new_cluster_list(
    name: str, clusters: list[ZigbeeClusterDesc]
) -> tuple[str, list[ZigbeeClusterDesc]]:
    """Create a cluster list array and return its name and the clusters."""
    # Always include basic and identify clusters first
    all_clusters = [
        ZigbeeClusterDesc(ZB_ZCL_CLUSTER_ID_BASIC, "zigbee_basic_attrib_list"),
        ZigbeeClusterDesc(ZB_ZCL_CLUSTER_ID_IDENTIFY, "zigbee_identify_attrib_list"),
    ]
    all_clusters.extend(clusters)

    cluster_strs = [str(c) for c in all_clusters]
    CORE.add_global(
        cg.RawExpression(
            f"zb_zcl_cluster_desc_t {name}[] = {{{', '.join(cluster_strs)}}}"
        )
    )
    return (name, all_clusters)


def zigbee_register_ep(
    ep_name: str,
    cluster_list_name: str,
    report_attr_count: int,
    clusters: list[ZigbeeClusterDesc],
    slot_index: int,
    app_device_id: str,
) -> None:
    """Register a Zigbee endpoint."""
    in_cluster_num = sum(1 for c in clusters if c.has_attrs)
    out_cluster_num = len(clusters) - in_cluster_num
    cluster_ids = [c.cluster_id for c in clusters]

    # Store endpoint name for device context generation
    CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER][slot_index] = ep_name

    # Generate the endpoint declaration
    ep_id = slot_index + 1  # Endpoints are 1-indexed
    obj = cg.RawExpression(
        f"ESPHOME_ZB_HA_DECLARE_EP({ep_name}, {ep_id}, {cluster_list_name}, "
        f"{in_cluster_num}, {out_cluster_num}, {report_attr_count}, {app_device_id}, {', '.join(cluster_ids)})"
    )
    CORE.add_global(obj)


@coroutine_with_priority(CoroPriority.LATE)
async def _ctx_to_code(config: ConfigType) -> None:
    cg.add_define("ZIGBEE_ENDPOINTS_COUNT", len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER]))
    cg.add_global(
        cg.RawExpression(
            f"ZBOSS_DECLARE_DEVICE_CTX_EP_VA(zb_device_ctx, &{', &'.join(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])})"
        )
    )
    cg.add(cg.RawExpression("ZB_AF_REGISTER_DEVICE_CTX(&zb_device_ctx)"))


async def zephyr_setup_binary_sensor(entity: cg.MockObj, config: ConfigType) -> None:
    CORE.add_job(_add_binary_sensor, entity, config)


async def zephyr_setup_sensor(entity: cg.MockObj, config: ConfigType) -> None:
    CORE.add_job(_add_sensor, entity, config)


async def zephyr_setup_switch(entity: cg.MockObj, config: ConfigType) -> None:
    CORE.add_job(_add_switch, entity, config)


def _slot_index() -> int:
    """Find the next available endpoint slot"""
    slot = next(
        (i for i, v in enumerate(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER]) if v == ""), None
    )
    if slot is None:
        raise cv.Invalid(
            f"Not found empty slot, size ({len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])})"
        )
    return slot


async def _add_zigbee_ep(
    entity: cg.MockObj,
    config: ConfigType,
    component_key,
    attrs_type,
    zcl_macro: str,
    cluster_id: str,
    app_device_id: str,
    extra_field_values: dict[str, int] | None = None,
) -> None:
    slot_index = _slot_index()

    prefix = f"zigbee_ep{slot_index + 1}"
    attrs_name = f"{prefix}_attrs"
    attr_list_name = f"{prefix}_attrib_list"
    cluster_list_name = f"{prefix}_cluster_list"
    ep_name = f"{prefix}_ep"

    # Create attribute struct
    attrs = zigbee_new_variable(attrs_name, attrs_type)

    # Build attribute list args
    attr_args = [
        zigbee_assign(attrs.out_of_service, 0),
        zigbee_assign(attrs.present_value, 0),
        zigbee_assign(attrs.status_flags, 0),
    ]
    # Add extra field assignments (e.g., engineering_units for sensors)
    if extra_field_values:
        for field_name, value in extra_field_values.items():
            attr_args.append(zigbee_assign(getattr(attrs, field_name), value))
    attr_args.append(zigbee_set_string(attrs.description, config[CONF_NAME]))

    # Create attribute list
    attr_list = zigbee_new_attr_list(attr_list_name, zcl_macro, *attr_args)

    # Create cluster list and register endpoint
    cluster_list_name, clusters = zigbee_new_cluster_list(
        cluster_list_name,
        [ZigbeeClusterDesc(cluster_id, attr_list)],
    )
    zigbee_register_ep(
        ep_name, cluster_list_name, 2, clusters, slot_index, app_device_id
    )

    # Create ESPHome component
    var = cg.new_Pvariable(config[component_key], entity)
    await cg.register_component(var, {})

    cg.add(var.set_endpoint(slot_index + 1))
    cg.add(var.set_cluster_attributes(attrs))

    hub = await cg.get_variable(config[CONF_ZIGBEE_ID])
    cg.add(var.set_parent(hub))


async def _add_binary_sensor(entity: cg.MockObj, config: ConfigType) -> None:
    await _add_zigbee_ep(
        entity,
        config,
        CONF_ZIGBEE_BINARY_SENSOR,
        BinaryAttrs,
        "ESPHOME_ZB_ZCL_DECLARE_BINARY_INPUT_ATTRIB_LIST",
        ZB_ZCL_CLUSTER_ID_BINARY_INPUT,
        "ZB_HA_SIMPLE_SENSOR_DEVICE_ID",
    )


async def _add_sensor(entity: cg.MockObj, config: ConfigType) -> None:
    # Get BACnet engineering unit from unit_of_measurement
    unit = config.get(CONF_UNIT_OF_MEASUREMENT, "")
    bacnet_unit = BACNET_UNITS.get(unit, BACNET_UNIT_NO_UNITS)

    await _add_zigbee_ep(
        entity,
        config,
        CONF_ZIGBEE_SENSOR,
        AnalogAttrs,
        "ESPHOME_ZB_ZCL_DECLARE_ANALOG_INPUT_ATTRIB_LIST",
        ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
        "ZB_HA_CUSTOM_ATTR_DEVICE_ID",
        extra_field_values={"engineering_units": bacnet_unit},
    )


async def _add_switch(entity: cg.MockObj, config: ConfigType) -> None:
    await _add_zigbee_ep(
        entity,
        config,
        CONF_ZIGBEE_SWITCH,
        BinaryAttrs,
        "ESPHOME_ZB_ZCL_DECLARE_BINARY_OUTPUT_ATTRIB_LIST",
        ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT,
        "ZB_HA_CUSTOM_ATTR_DEVICE_ID",
    )
