import esphome.codegen as cg
from esphome.components import ble_client, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALARM,
    CONF_ID,
    CONF_RADON,
    CONF_RADON_DAY_AVG,
    CONF_RADON_LONG_TERM,
    CONF_RADON_PEAK,
    CONF_VERSION,
    ICON_ALARM_LIGHT,
    ICON_RADIOACTIVE,
    STATE_CLASS_MEASUREMENT,
    UNIT_BECQUEREL_PER_CUBIC_METER,
    UNIT_EMPTY,
)

DEPENDENCIES = ["ble_client"]

radon_eye_rd200_ns = cg.esphome_ns.namespace("radon_eye_rd200")
RadonEyeRD200 = radon_eye_rd200_ns.class_(
    "RadonEyeRD200", cg.PollingComponent, ble_client.BLEClientNode
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RadonEyeRD200),
            cv.Optional(CONF_VERSION, default=1): cv.int_range(1, 3),
            cv.Optional(CONF_ALARM): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_ALARM_LIGHT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RADON): sensor.sensor_schema(
                unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
                icon=ICON_RADIOACTIVE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RADON_LONG_TERM): sensor.sensor_schema(
                unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
                icon=ICON_RADIOACTIVE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RADON_PEAK): sensor.sensor_schema(
                unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
                icon=ICON_RADIOACTIVE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RADON_DAY_AVG): sensor.sensor_schema(
                unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
                icon=ICON_RADIOACTIVE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("5min"))
    .extend(ble_client.BLE_CLIENT_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await ble_client.register_ble_node(var, config)

    cg.add(var.set_version(config[CONF_VERSION]))

    if CONF_ALARM in config:
        sens = await sensor.new_sensor(config[CONF_ALARM])
        cg.add(var.set_alarm(sens))
    if CONF_RADON in config:
        sens = await sensor.new_sensor(config[CONF_RADON])
        cg.add(var.set_radon(sens))
    if CONF_RADON_LONG_TERM in config:
        sens = await sensor.new_sensor(config[CONF_RADON_LONG_TERM])
        cg.add(var.set_radon_long_term(sens))
    if CONF_RADON_PEAK in config:
        sens = await sensor.new_sensor(config[CONF_RADON_PEAK])
        cg.add(var.set_radon_peak(sens))
    if CONF_RADON_DAY_AVG in config:
        sens = await sensor.new_sensor(config[CONF_RADON_DAY_AVG])
        cg.add(var.set_radon_day_avg(sens))
