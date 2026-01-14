import logging
from typing import Any

from esphome import automation, core
import esphome.codegen as cg
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG, Section
from esphome.components.zephyr import zephyr_add_pm_static, zephyr_data
from esphome.components.zephyr.const import KEY_BOOTLOADER
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERNAL, CONF_NAME
from esphome.core import CORE
from esphome.types import ConfigType

from .const_zephyr import (
    CONF_MAX_EP_NUMBER,
    CONF_ON_JOIN,
    CONF_POWER_SOURCE,
    CONF_WIPE_ON_BOOT,
    CONF_ZIGBEE_ID,
    KEY_EP_NUMBER,
    KEY_ZIGBEE,
    POWER_SOURCE,
    ZigbeeComponent,
    zigbee_ns,
)
from .zigbee_zephyr import zephyr_binary_sensor, zephyr_sensor, zephyr_switch

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@tomaszduda23"]


def zigbee_set_core_data(config: ConfigType) -> ConfigType:
    if zephyr_data()[KEY_BOOTLOADER] in BOOTLOADER_CONFIG:
        zephyr_add_pm_static(
            [Section("empty_after_zboss_offset", 0xF4000, 0xC000, "flash_primary")]
        )

    return config


BINARY_SENSOR_SCHEMA = cv.Schema({}).extend(zephyr_binary_sensor)
SENSOR_SCHEMA = cv.Schema({}).extend(zephyr_sensor)
SWITCH_SCHEMA = cv.Schema({}).extend(zephyr_switch)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(ZigbeeComponent),
            cv.Optional(CONF_ON_JOIN): automation.validate_automation(single=True),
            cv.Optional(CONF_WIPE_ON_BOOT, default=False): cv.All(
                cv.Any(
                    cv.boolean,
                    cv.one_of(*["once"], lower=True),
                ),
                cv.requires_component("nrf52"),
            ),
            cv.Optional(CONF_POWER_SOURCE, default="DC_SOURCE"): cv.enum(
                POWER_SOURCE, upper=True
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
    if count > CONF_MAX_EP_NUMBER and not CORE.testing_mode:
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


async def setup_sensor(entity: cg.MockObj, config: ConfigType) -> None:
    if not config.get(CONF_ZIGBEE_ID) or config.get(CONF_INTERNAL):
        return
    if CORE.using_zephyr:
        from .zigbee_zephyr import zephyr_setup_sensor

        await zephyr_setup_sensor(entity, config)


async def setup_switch(entity: cg.MockObj, config: ConfigType) -> None:
    if not config.get(CONF_ZIGBEE_ID) or config.get(CONF_INTERNAL):
        return
    if CORE.using_zephyr:
        from .zigbee_zephyr import zephyr_setup_switch

        await zephyr_setup_switch(entity, config)


def consume_endpoint(config: ConfigType) -> ConfigType:
    if not config.get(CONF_ZIGBEE_ID) or config.get(CONF_INTERNAL):
        return config
    if " " in config[CONF_NAME]:
        _LOGGER.warning(
            "Spaces in '%s' work with ZHA but not Zigbee2MQTT. For Zigbee2MQTT use '%s'",
            config[CONF_NAME],
            config[CONF_NAME].replace(" ", "_"),
        )
    data: dict[str, Any] = CORE.data.setdefault(KEY_ZIGBEE, {})
    slots: list[str] = data.setdefault(KEY_EP_NUMBER, [])
    slots.extend([""])
    return config


def validate_binary_sensor(config: ConfigType) -> ConfigType:
    return consume_endpoint(config)


def validate_sensor(config: ConfigType) -> ConfigType:
    return consume_endpoint(config)


def validate_switch(config: ConfigType) -> ConfigType:
    return consume_endpoint(config)


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
