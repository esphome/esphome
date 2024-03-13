import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.canbus import (
    CONF_CANBUS_ID,
    CanbusComponent,
    CanbusTrigger,
    CANBUS_DEVICE_SCHEMA,
)
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
    CONF_NAME,
    CONF_DEBUG,
    CONF_THROTTLE,
    CONF_TIMEOUT,
)

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["canbus"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor"]
MULTI_CONF = True

CONF_BMS_ID = "bms_id"
CONF_SCALE = "scale"
CONF_MSG_ID = "msg_id"
CONF_BIT_NO = "bit_no"
CONF_WARNINGS = "warnings"
CONF_ALARMS = "alarms"

bms = cg.esphome_ns.namespace("canbus_bms")
BmsComponent = bms.class_("CanbusBmsComponent", cg.PollingComponent, CanbusComponent)
BmsTrigger = bms.class_("BmsTrigger", CanbusTrigger)
SensorDesc = bms.class_("SensorDesc")
BinarySensorDesc = bms.class_("BinarySensorDesc")
TextSensorDesc = bms.class_("TextSensorDesc")
FlagDesc = bms.class_("FlagDesc")


def throttle_before_timeout(config):
    if config[CONF_THROTTLE] >= config[CONF_TIMEOUT]:
        raise cv.Invalid("throttle interval must be less than timeout")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BmsComponent),
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BmsTrigger),
            cv.Optional(CONF_DEBUG, default=False): cv.boolean,
            cv.Optional(CONF_THROTTLE, default="15s"): cv.positive_time_period,
            cv.Optional(CONF_TIMEOUT, default="60s"): cv.positive_time_period,
            cv.Optional(CONF_NAME, default="CanbusBMS"): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(CANBUS_DEVICE_SCHEMA),
    throttle_before_timeout,
)


async def to_code(config):
    canbus = await cg.get_variable(config[CONF_CANBUS_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_THROTTLE].total_milliseconds,
        config[CONF_TIMEOUT].total_milliseconds,
        config[CONF_NAME],
        config[CONF_DEBUG],
    )
    await cg.register_component(var, config)
    cg.add(var.set_canbus(canbus))
    trigger = cg.new_Pvariable(config[CONF_TRIGGER_ID], var, canbus)
    await cg.register_component(trigger, config)
