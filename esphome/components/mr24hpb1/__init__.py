import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import uart
from esphome.const import (
    CONF_ID,
)

CODEOWNERS = ["@lorki97", "@florianL21"]

DEPENDENCIES = ["uart"]

mr24hpb1_ns = cg.esphome_ns.namespace("mr24hpb1")

CONF_MR24HPB1_ID = "mr24hpb1_id"

MR24HPB1Component = mr24hpb1_ns.class_(
    "MR24HPB1Component", cg.Component, uart.UARTDevice
)

# Scene Setting enum
CONF_SCENE_SETTING = "scene_setting"
MR24HPB1SceneSetting = mr24hpb1_ns.enum("SceneSetting")
SCENE_SETTING = {
    "DEFAULT": MR24HPB1SceneSetting.SCENE_DEFAULT,
    "AREA": MR24HPB1SceneSetting.AREA,
    "BATHROOM": MR24HPB1SceneSetting.BATHROOM,
    "BEDROOM": MR24HPB1SceneSetting.BEDROOM,
    "LIVING_ROOM": MR24HPB1SceneSetting.LIVING_ROOM,
    "OFFICE": MR24HPB1SceneSetting.OFFICE,
    "HOTEL": MR24HPB1SceneSetting.HOTEL,
}

# Threshold gear
CONF_THRESHOLD_GEAR = "threshold_gear"

# Forced unoccupied enum
CONF_FORCE_UNOCCUPIED = "force_unoccupied"
MR24HPB1ForcedUnoccupied = mr24hpb1_ns.enum("ForcedUnoccupied", True)
FORCED_UNOCCUPIED = {
    "NONE": MR24HPB1ForcedUnoccupied.NONE,
    "10S": MR24HPB1ForcedUnoccupied.SEC_10,
    "30S": MR24HPB1ForcedUnoccupied.SEC_30,
    "1MIN": MR24HPB1ForcedUnoccupied.MIN_1,
    "2MIN": MR24HPB1ForcedUnoccupied.MIN_2,
    "5MIN": MR24HPB1ForcedUnoccupied.MIN_5,
    "10MIN": MR24HPB1ForcedUnoccupied.MIN_10,
    "30MIN": MR24HPB1ForcedUnoccupied.MIN_30,
    "60MIN": MR24HPB1ForcedUnoccupied.MIN_60,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MR24HPB1Component),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_SCENE_SETTING, default="DEFAULT"): cv.enum(
                SCENE_SETTING, upper=True, space="_"
            ),
            cv.Optional(CONF_FORCE_UNOCCUPIED, default="NONE"): cv.enum(
                FORCED_UNOCCUPIED, upper=True, space="_"
            ),
            cv.Optional(CONF_THRESHOLD_GEAR, default=7): cv.int_range(1, 10),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_scene_setting(config[CONF_SCENE_SETTING]))
    cg.add(var.set_forced_unoccupied(config[CONF_FORCE_UNOCCUPIED]))
    cg.add(var.set_threshold_gear(config[CONF_THRESHOLD_GEAR]))
