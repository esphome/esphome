import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_RANGE,
    CONF_MIN_RANGE,
    DEVICE_CLASS_DISTANCE,
    ENTITY_CATEGORY_CONFIG,
)

from .. import CONF_C4002_ID, C4002Component, dfrobot_c4002_ns
from .const import (
    CONF_AREA1_MAX,
    CONF_AREA1_MIN,
    CONF_AREA2_MAX,
    CONF_AREA2_MIN,
    CONF_AREA3_MAX,
    CONF_AREA3_MIN,
    CONF_LIGHT_THRESHOLD_1,
)

CONF_TARGET_DISAPPEARD_Delay_TIME = "target_disappeard_delay_time"

MinDetectRangeNumber = dfrobot_c4002_ns.class_("MinDetectRangeNumber", number.Number)
MaxRDetectangeNumber = dfrobot_c4002_ns.class_("MaxDetectRangeNumber", number.Number)
LightThresholdNumber = dfrobot_c4002_ns.class_("LightThresholdNumber", number.Number)

Area1MinRangeNumber = dfrobot_c4002_ns.class_("Area1MinRangeNumber", number.Number)
Area1MaxRangeNumber = dfrobot_c4002_ns.class_("Area1MaxRangeNumber", number.Number)

Area2MinRangeNumber = dfrobot_c4002_ns.class_("Area2MinRangeNumber", number.Number)
Area2MaxRangeNumber = dfrobot_c4002_ns.class_("Area2MaxRangeNumber", number.Number)

Area3MinRangeNumber = dfrobot_c4002_ns.class_("Area3MinRangeNumber", number.Number)
Area3MaxRangeNumber = dfrobot_c4002_ns.class_("Area3MaxRangeNumber", number.Number)
TargetDisappeardDelayTimeNumber = dfrobot_c4002_ns.class_(
    "TargetDisappeardDelayTimeNumber", number.Number
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_C4002_ID): cv.use_id(C4002Component),
        cv.Optional(CONF_MIN_RANGE): number.number_schema(
            MinDetectRangeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:ruler",
            unit_of_measurement="m",
        ),
        cv.Optional(CONF_MAX_RANGE): number.number_schema(
            MaxRDetectangeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:ruler",
            unit_of_measurement="m",
        ),
        cv.Optional("light_threshold"): number.number_schema(
            LightThresholdNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:lightbulb",
            unit_of_measurement="lx",
        ),
        cv.Optional(CONF_AREA1_MIN): number.number_schema(
            Area1MinRangeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:counter",
            unit_of_measurement="m",
        ),
        cv.Optional(CONF_AREA1_MAX): number.number_schema(
            Area1MaxRangeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:counter",
            unit_of_measurement="m",
        ),
        cv.Optional(CONF_AREA2_MIN): number.number_schema(
            Area2MinRangeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:counter",
            unit_of_measurement="m",
        ),
        cv.Optional(CONF_AREA2_MAX): number.number_schema(
            Area2MaxRangeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:counter",
            unit_of_measurement="m",
        ),
        cv.Optional(CONF_AREA3_MIN): number.number_schema(
            Area3MinRangeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:counter",
            unit_of_measurement="m",
        ),
        cv.Optional(CONF_AREA3_MAX): number.number_schema(
            Area3MaxRangeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:counter",
            unit_of_measurement="m",
        ),
        cv.Optional(CONF_TARGET_DISAPPEARD_Delay_TIME): number.number_schema(
            TargetDisappeardDelayTimeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:account-clock",
            unit_of_measurement="s",
        ),
    }
)


async def to_code(config):
    number_component = await cg.get_variable(config[CONF_C4002_ID])
    # 最小探测距离
    if min_config := config.get(CONF_MIN_RANGE):
        n = await number.new_number(min_config, min_value=0, max_value=11.0, step=0.1)
        await cg.register_parented(n, config[CONF_C4002_ID])
        cg.add(number_component.set_min_range_number(n))
    # 最大探测距离
    if max_config := config.get(CONF_MAX_RANGE):
        n = await number.new_number(max_config, min_value=0.8, max_value=11.0, step=0.1)
        await cg.register_parented(n, config[CONF_C4002_ID])
        cg.add(number_component.set_max_range_number(n))
    # 光照阈值
    if light_threshold_config := config.get(CONF_LIGHT_THRESHOLD_1):
        light_threshold = await number.new_number(
            light_threshold_config, min_value=0, max_value=60, step=0.1
        )
        await cg.register_parented(light_threshold, config[CONF_C4002_ID])
        cg.add(number_component.set_light_threshold_number(light_threshold))
    # 区域 1 最小范围
    if area1_min_config := config.get(CONF_AREA1_MIN):
        n = await number.new_number(
            area1_min_config, min_value=0, max_value=11.6, step=0.8
        )
        await cg.register_parented(n, config[CONF_C4002_ID])
        cg.add(number_component.set_area1_min_range_number(n))

    # 区域 1 最大范围
    if area1_max_config := config.get(CONF_AREA1_MAX):
        n = await number.new_number(
            area1_max_config, min_value=0, max_value=11.6, step=0.8
        )
        await cg.register_parented(n, config[CONF_C4002_ID])
        cg.add(number_component.set_area1_max_range_number(n))

    # 区域 2 最小范围
    if area2_min_config := config.get(CONF_AREA2_MIN):
        n = await number.new_number(
            area2_min_config, min_value=0, max_value=11.6, step=0.8
        )
        await cg.register_parented(n, config[CONF_C4002_ID])
        cg.add(number_component.set_area2_min_range_number(n))

    # 区域 2 最大范围
    if area2_max_config := config.get(CONF_AREA2_MAX):
        n = await number.new_number(
            area2_max_config, min_value=0, max_value=11.6, step=0.8
        )
        await cg.register_parented(n, config[CONF_C4002_ID])
        cg.add(number_component.set_area2_max_range_number(n))

    # 区域 3 最小范围
    if area3_min_config := config.get(CONF_AREA3_MIN):
        n = await number.new_number(
            area3_min_config, min_value=0, max_value=11.6, step=0.8
        )
        await cg.register_parented(n, config[CONF_C4002_ID])
        cg.add(number_component.set_area3_min_range_number(n))

    # 区域 3 最大范围
    if area3_max_config := config.get(CONF_AREA3_MAX):
        n = await number.new_number(
            area3_max_config, min_value=0, max_value=11.6, step=0.8
        )
        await cg.register_parented(n, config[CONF_C4002_ID])
        cg.add(number_component.set_area3_max_range_number(n))

    if target_disappeard_delay_time_config := config.get(
        CONF_TARGET_DISAPPEARD_Delay_TIME
    ):
        n = await number.new_number(
            target_disappeard_delay_time_config, min_value=0, max_value=100, step=1
        )
        await cg.register_parented(n, config[CONF_C4002_ID])
        cg.add(number_component.set_target_disappeard_delay_time_number(n))
