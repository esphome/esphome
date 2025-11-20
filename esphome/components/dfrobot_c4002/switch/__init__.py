import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG

from .. import CONF_C4002_ID, C4002Component, dfrobot_c4002_ns

CONF_SWITCH_OUT_LED = "switch_out_led"
CONF_SWITCH_RUN_LED = "switch_run_led"
CONF_SWITCH_FACTORY_RESET = "switch_factory_reset"
CONF_SWITCH_ENVIRONMENTAL_CALIBRATION = "switch_environmental_calibration"

C4002Switch1 = dfrobot_c4002_ns.class_("C4002Switch1", switch.Switch)
C4002Switch2 = dfrobot_c4002_ns.class_("C4002Switch2", switch.Switch)
C4002SwitchFactoryReset = dfrobot_c4002_ns.class_(
    "C4002SwitchFactoryReset", switch.Switch
)
C4002SwitchEnvironmentalCalibration = dfrobot_c4002_ns.class_(
    "C4002SwitchEnvironmentalCalibration", switch.Switch
)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_C4002_ID): cv.use_id(C4002Component),
    cv.Optional(CONF_SWITCH_OUT_LED): switch.switch_schema(
        C4002Switch1,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:lightbulb",  # 灯泡图标
    ),
    cv.Optional(CONF_SWITCH_RUN_LED): switch.switch_schema(
        C4002Switch2,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:lightbulb",  # 另一个灯泡图标
    ),
    cv.Optional(CONF_SWITCH_FACTORY_RESET): switch.switch_schema(
        C4002SwitchFactoryReset,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:restart",
    ),
    cv.Optional(CONF_SWITCH_ENVIRONMENTAL_CALIBRATION): switch.switch_schema(
        C4002SwitchEnvironmentalCalibration,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:leaf",
    ),
}


async def to_code(config):
    switch_component = await cg.get_variable(config[CONF_C4002_ID])

    # 配置第一个开关
    if switch1_config := config.get(CONF_SWITCH_OUT_LED):
        sw1 = await switch.new_switch(switch1_config)
        await cg.register_parented(sw1, config[CONF_C4002_ID])
        cg.add(switch_component.set_out_led_switch(sw1))  # 需要在组件中添加对应方法

    # 配置第二个开关
    if switch2_config := config.get(CONF_SWITCH_RUN_LED):
        sw2 = await switch.new_switch(switch2_config)
        await cg.register_parented(sw2, config[CONF_C4002_ID])
        cg.add(switch_component.set_run_led_switch(sw2))  # 需要在组件中添加对应方法

    if factory_reset_config := config.get(CONF_SWITCH_FACTORY_RESET):
        sw_factory_reset = await switch.new_switch(factory_reset_config)
        await cg.register_parented(sw_factory_reset, config[CONF_C4002_ID])
        cg.add(
            switch_component.set_factory_reset_switch(sw_factory_reset)
        )  # 需要在组件中添加对应方法

    if environmental_calibration_config := config.get(
        CONF_SWITCH_ENVIRONMENTAL_CALIBRATION
    ):
        sw_environmental_calibration = await switch.new_switch(
            environmental_calibration_config
        )
        await cg.register_parented(sw_environmental_calibration, config[CONF_C4002_ID])
        cg.add(
            switch_component.set_environmental_calibration_switch(
                sw_environmental_calibration
            )
        )  # 需要在组件中添加对应方法
