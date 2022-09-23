import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import uart, text_sensor, binary_sensor, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_OCCUPANCY,
    DEVICE_CLASS_MOTION,
    ICON_MOTION_SENSOR,
)

CODEOWNERS = ["lorki97", "florianL21"]

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["text_sensor", "binary_sensor", "sensor"]

mr24hpb1_ns = cg.esphome_ns.namespace("mr24hpb1")

CONF_DEVICE_ID = "device_id"
CONF_SOFTWARE_VERSION = "software_version"
CONF_HARDWARE_VERSION = "hardware_version"
CONF_PROTOCOL_VERSION = "protocol_version"

MR24HPB1Component = mr24hpb1_ns.class_(
    "MR24HPB1Component", cg.Component, uart.UARTDevice
)

# Enviroment status
CONF_ENV_STATUS = "environment_status"

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

# Occupancy binary sensor
CONF_OCCUPANCY_SENSOR = "occupancy"

# Movment binary sensor
CONF_MOVEMENT_DETECTED_SENSOR = "movement"

# Movement rate
CONF_MOVEMENT_SENSOR = "movement_rate"

# Forced unoccupied enum
CONF_FORCED_UNOCCUPIED = "force_unoccupied"
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

# Movment type text sensor
CONF_MOVEMENT_TYPE = "movement_type"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MR24HPB1Component),
            cv.Optional(CONF_DEVICE_ID): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_SOFTWARE_VERSION): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_HARDWARE_VERSION): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_PROTOCOL_VERSION): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_ENV_STATUS): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_OCCUPANCY_SENSOR): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_OCCUPANCY,
            ),
            cv.Optional(
                CONF_MOVEMENT_DETECTED_SENSOR
            ): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_MOTION,
                icon=ICON_MOTION_SENSOR,
            ),
            cv.Optional(CONF_MOVEMENT_SENSOR): sensor.sensor_schema(),
            cv.Optional(CONF_MOVEMENT_TYPE): text_sensor.text_sensor_schema(),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_SCENE_SETTING, default="DEFAULT"): cv.enum(
                SCENE_SETTING, upper=True, space="_"
            ),
            cv.Optional(CONF_FORCED_UNOCCUPIED, default="NONE"): cv.enum(
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
    cg.add(var.set_forced_unoccupied(config[CONF_FORCED_UNOCCUPIED]))
    cg.add(var.set_threshold_gear(config[CONF_THRESHOLD_GEAR]))

    if CONF_DEVICE_ID in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DEVICE_ID])
        cg.add(var.set_device_id_sensor(sens))

    if CONF_SOFTWARE_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SOFTWARE_VERSION])
        cg.add(var.set_software_version_sensor(sens))

    if CONF_HARDWARE_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_HARDWARE_VERSION])
        cg.add(var.set_hardware_version_sensor(sens))

    if CONF_PROTOCOL_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_PROTOCOL_VERSION])
        cg.add(var.set_protocol_version_sensor(sens))

    if CONF_ENV_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_ENV_STATUS])
        cg.add(var.set_environment_status_sensor(sens))

    if CONF_OCCUPANCY_SENSOR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_OCCUPANCY_SENSOR])
        cg.add(var.set_occupancy_sensor(sens))

    if CONF_MOVEMENT_DETECTED_SENSOR in config:
        sens = await binary_sensor.new_binary_sensor(
            config[CONF_MOVEMENT_DETECTED_SENSOR]
        )
        cg.add(var.set_movement_sensor(sens))

    if CONF_MOVEMENT_SENSOR in config:
        sens = await sensor.new_sensor(config[CONF_MOVEMENT_SENSOR])
        cg.add(var.set_movement_rate_sensor(sens))

    if CONF_MOVEMENT_TYPE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_MOVEMENT_TYPE])
        cg.add(var.set_movement_type_sensor(sens))
