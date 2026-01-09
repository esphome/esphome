import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SPEED,
    ICON_RULER,
    UNIT_METER,
)

from .. import CONF_C4002_ID, C4002Component, dfrobot_c4002_ns

C4002Sensor = dfrobot_c4002_ns.class_("C4002Sensor", cg.Component)

CONF_MOVEMENT_DISTANCE = "movement_distance"
CONF_EXISTING_DISTANCE = "existing_distance"
CONF_MOVEMENT_SPEED = "movement_speed"
CONF_MOVEMENT_DIRECTION = "movement_direction"
CONF_TARGET_STATUS = "target_status"


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(C4002Sensor),
        cv.Required(CONF_C4002_ID): cv.use_id(C4002Component),
        cv.Optional(CONF_MOVEMENT_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_METER,
            icon=ICON_RULER,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_EXISTING_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_METER,
            icon=ICON_RULER,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_MOVEMENT_SPEED): sensor.sensor_schema(
            device_class=DEVICE_CLASS_SPEED,
            unit_of_measurement="m/s",
            icon="mdi:speedometer",
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_MOVEMENT_DIRECTION): sensor.sensor_schema(
            icon="mdi:compass",
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_TARGET_STATUS): sensor.sensor_schema(
            icon="mdi:target",
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    c4002_sensor = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(c4002_sensor, config)

    # 运动距离传感器
    if CONF_MOVEMENT_DISTANCE in config:
        sens_conf = config[CONF_MOVEMENT_DISTANCE]
        sens = await sensor.new_sensor(sens_conf)
        cg.add(c4002_sensor.set_movement_distance_sensor(sens))

    # 存在距离传感器
    if CONF_EXISTING_DISTANCE in config:
        sens_conf = config[CONF_EXISTING_DISTANCE]
        sens = await sensor.new_sensor(sens_conf)
        cg.add(c4002_sensor.set_existing_distance_sensor(sens))

    # 运动速度传感器
    if CONF_MOVEMENT_SPEED in config:
        sens_conf = config[CONF_MOVEMENT_SPEED]
        sens = await sensor.new_sensor(sens_conf)
        cg.add(c4002_sensor.set_movement_speed_sensor(sens))

    # 运动方向传感器
    if CONF_MOVEMENT_DIRECTION in config:
        sens_conf = config[CONF_MOVEMENT_DIRECTION]
        sens = await sensor.new_sensor(sens_conf)
        cg.add(c4002_sensor.set_movement_direction_sensor(sens))

    # 目标状态传感器
    if CONF_TARGET_STATUS in config:
        sens_conf = config[CONF_TARGET_STATUS]
        sens = await sensor.new_sensor(sens_conf)
        cg.add(c4002_sensor.set_target_status_sensor(sens))

    c4002_component = await cg.get_variable(config[CONF_C4002_ID])
    cg.add(c4002_component.register_listener(c4002_sensor))
