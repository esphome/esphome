import esphome.codegen as cg
from esphome.components import light, switch
import esphome.config_validation as cv
from esphome.const import CONF_DEFAULT_TRANSITION_LENGTH, CONF_ID, CONF_OUTPUT_ID

from .. import FendtSwitch, fendt_caravan_ns

LightingDeviceSensor = fendt_caravan_ns.class_(
    "LightingDeviceSensor", cg.PollingComponent
)
FendtLightOutput = fendt_caravan_ns.class_("FendtLightOutput", light.LightOutput)

CONF_LIGHTING_DEVICE = "lighting_device"

CONF_LIGHT_SW0 = "light_sw0"
CONF_LIGHT_SW1 = "light_sw1"
CONF_LIGHT_SW2 = "light_sw2"
CONF_LIGHT_SW3 = "light_sw3"
CONF_LIGHT_DIMSW0 = "light_dimsw0"
CONF_LIGHT_DIMSW1 = "light_dimsw1"
CONF_LIGHT_DIMSW2 = "light_dimsw2"
CONF_LIGHT_DIMSW3 = "light_dimsw3"
CONF_LIGHT_DIMSW4 = "light_dimsw4"

LIGHTINGS = {
    CONF_LIGHT_SW0,
    CONF_LIGHT_SW1,
    CONF_LIGHT_SW2,
    CONF_LIGHT_SW3,
    CONF_LIGHT_DIMSW0,
    CONF_LIGHT_DIMSW1,
    CONF_LIGHT_DIMSW2,
    CONF_LIGHT_DIMSW3,
    CONF_LIGHT_DIMSW4,
}

CONF_LIGHT_DIM_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(FendtLightOutput),
        cv.Optional(
            CONF_DEFAULT_TRANSITION_LENGTH, default="0s"
        ): cv.positive_time_period_milliseconds,
    }
)

LIGHTING_TYPES = {
    CONF_LIGHT_SW0: switch.switch_schema(
        FendtSwitch, default_restore_mode="RESTORE_DEFAULT_OFF", icon="mdi:lamp"
    ),
    CONF_LIGHT_SW1: switch.switch_schema(
        FendtSwitch, default_restore_mode="RESTORE_DEFAULT_OFF", icon="mdi:lamp"
    ),
    CONF_LIGHT_SW2: switch.switch_schema(
        FendtSwitch, default_restore_mode="RESTORE_DEFAULT_OFF", icon="mdi:lamp"
    ),
    CONF_LIGHT_SW3: switch.switch_schema(
        FendtSwitch, default_restore_mode="RESTORE_DEFAULT_OFF", icon="mdi:lamp"
    ),
    CONF_LIGHT_DIMSW0: CONF_LIGHT_DIM_SCHEMA,
    CONF_LIGHT_DIMSW1: CONF_LIGHT_DIM_SCHEMA,
    CONF_LIGHT_DIMSW2: CONF_LIGHT_DIM_SCHEMA,
    CONF_LIGHT_DIMSW3: CONF_LIGHT_DIM_SCHEMA,
    CONF_LIGHT_DIMSW4: CONF_LIGHT_DIM_SCHEMA,
}

CONFIG_LIGHTING_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(LightingDeviceSensor),
        **{cv.Optional(type): schema for type, schema in LIGHTING_TYPES.items()},
    }
).extend(cv.polling_component_schema("60s"))
