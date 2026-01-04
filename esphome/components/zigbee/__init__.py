from typing import Any

from esphome import automation, core
import esphome.codegen as cg
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG, Section
from esphome.components.zephyr import zephyr_add_pm_static, zephyr_data
from esphome.components.zephyr.const import KEY_BOOTLOADER
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERNAL
from esphome.core import CORE
from esphome.types import ConfigType

from .const_zephyr import (
    CONF_MAX_EP_NUMBER,
    CONF_ON_JOIN,
    CONF_WIPE_ON_BOOT,
    CONF_ZIGBEE_ID,
    KEY_EP_NUMBER,
    KEY_ZIGBEE,
    ZigbeeComponent,
    zigbee_ns,
)
from .zigbee_zephyr import zephyr_binary_sensor

CODEOWNERS = ["@tomaszduda23"]


def zigbee_set_core_data(config: ConfigType) -> ConfigType:
    if zephyr_data()[KEY_BOOTLOADER] in BOOTLOADER_CONFIG:
        zephyr_add_pm_static(
            [Section("empty_after_zboss_offset", 0xF4000, 0xC000, "flash_primary")]
        )

    return config


BINARY_SENSOR_SCHEMA = cv.Schema({}).extend(zephyr_binary_sensor)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(ZigbeeComponent),
            cv.Optional(CONF_ON_JOIN): automation.validate_automation(single=True),
            cv.Optional(CONF_WIPE_ON_BOOT, default=False): cv.All(
                cv.boolean,
                cv.requires_component("nrf52"),
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    zigbee_set_core_data,
    cv.only_with_framework("zephyr"),
)


def validate_number_of_ep(config: ConfigType) -> None:
    if KEY_ZIGBEE not in CORE.data:
        raise cv.Invalid("At least one zigbee device need to be included")
    count = len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])
    if count == 1:
        raise cv.Invalid(
            "Single endpoint is not supported https://github.com/Koenkk/zigbee2mqtt/issues/29888"
        )
    if count > CONF_MAX_EP_NUMBER:
        raise cv.Invalid(f"Maximum number of end points is {CONF_MAX_EP_NUMBER}")


FINAL_VALIDATE_SCHEMA = cv.All(
    validate_number_of_ep,
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_ZIGBEE")
    if CORE.using_zephyr:
        from .zigbee_zephyr import zephyr_to_code

        await zephyr_to_code(config)


async def setup_binary_sensor(entity: cg.MockObj, config: ConfigType) -> None:
    if not config.get(CONF_ZIGBEE_ID) or config.get(CONF_INTERNAL):
        return
    if CORE.using_zephyr:
        from .zigbee_zephyr import zephyr_setup_binary_sensor

        await zephyr_setup_binary_sensor(entity, config)


def validate_binary_sensor(config: ConfigType) -> ConfigType:
    if not config.get(CONF_ZIGBEE_ID) or config.get(CONF_INTERNAL):
        return config
    data: dict[str, Any] = CORE.data.setdefault(KEY_ZIGBEE, {})
    slots: list[str] = data.setdefault(KEY_EP_NUMBER, [])
    slots.extend([""])
    return config


ZIGBEE_ACTION_SCHEMA = automation.maybe_simple_id(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(ZigbeeComponent),
        }
    )
)

FactoryResetAction = zigbee_ns.class_(
    "FactoryResetAction", automation.Action, cg.Parented.template(ZigbeeComponent)
)


@automation.register_action(
    "zigbee.factory_reset",
    FactoryResetAction,
    ZIGBEE_ACTION_SCHEMA,
)
async def reset_zigbee_to_code(
    config: ConfigType,
    action_id: core.ID,
    template_arg: cg.TemplateArguments,
    args: list[tuple],
) -> cg.Pvariable:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
