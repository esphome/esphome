import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TYPE,
    DEVICE_CLASS_BATTERY,
    ICON_BATTERY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

from .. import CONF_BATTERY_GAUGE_ID, BatteryGauge

DEPENDENCIES = ["battery_gauge"]

TYPE_STATE_OF_CHARGE = "state_of_charge"

# Maps a `type:` value to the BatteryGauge setter that attaches this sensor to the hub. One entry
# today; a future time-to-full reading is just another key here plus its own sensor_schema call.
SENSOR_TYPES = {
    TYPE_STATE_OF_CHARGE: "set_state_of_charge_sensor",
}

CONFIG_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement=UNIT_PERCENT,
    state_class=STATE_CLASS_MEASUREMENT,
    device_class=DEVICE_CLASS_BATTERY,
    accuracy_decimals=1,
    icon=ICON_BATTERY,
).extend(
    {
        cv.GenerateID(CONF_BATTERY_GAUGE_ID): cv.use_id(BatteryGauge),
        cv.Optional(CONF_TYPE, default=TYPE_STATE_OF_CHARGE): cv.one_of(
            *SENSOR_TYPES, lower=True
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BATTERY_GAUGE_ID])
    var = await sensor.new_sensor(config)
    setter = SENSOR_TYPES[config[CONF_TYPE]]
    cg.add(getattr(hub, setter)(var))
