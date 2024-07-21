import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, time
from esphome.const import (
    CONF_TIME_ID,
    DEVICE_CLASS_TIMESTAMP,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_SECOND,
    ICON_TIMER,
    DEVICE_CLASS_DURATION,
)

uptime_ns = cg.esphome_ns.namespace("uptime")
UptimeSecondsSensor = uptime_ns.class_(
    "UptimeSecondsSensor", sensor.Sensor, cg.PollingComponent
)
UptimeTimestampSensor = uptime_ns.class_(
    "UptimeTimestampSensor", sensor.Sensor, cg.Component
)


CONFIG_SCHEMA = cv.typed_schema(
    {
        "seconds": sensor.sensor_schema(
            UptimeSecondsSensor,
            unit_of_measurement=UNIT_SECOND,
            icon=ICON_TIMER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            device_class=DEVICE_CLASS_DURATION,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("60s")),
        "timestamp": sensor.sensor_schema(
            UptimeTimestampSensor,
            icon=ICON_TIMER,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TIMESTAMP,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        )
        .extend(
            cv.Schema(
                {
                    cv.GenerateID(CONF_TIME_ID): cv.All(
                        cv.requires_component("time"), cv.use_id(time.RealTimeClock)
                    ),
                }
            )
        )
        .extend(cv.COMPONENT_SCHEMA),
    },
    default_type="seconds",
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    if time_id_config := config.get(CONF_TIME_ID):
        time_id = await cg.get_variable(time_id_config)
        cg.add(var.set_time(time_id))
