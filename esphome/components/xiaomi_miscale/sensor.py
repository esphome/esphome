import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import (
    CONF_MAC_ADDRESS,
    CONF_ID,
    CONF_WEIGHT,
    STATE_CLASS_MEASUREMENT,
    UNIT_KILOGRAM,
    ICON_SCALE_BATHROOM,
    UNIT_OHM,
    CONF_IMPEDANCE,
    ICON_OMEGA,
    CONF_VERSION,
)

DEPENDENCIES = ["esp32_ble_tracker"]

xiaomi_miscale_ns = cg.esphome_ns.namespace("xiaomi_miscale")
XiaomiMiscale = xiaomi_miscale_ns.class_(
    "XiaomiMiscale", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)


def validate_sensor_match_version(config):
    version = int(config.get(CONF_VERSION, 1))
    has_impedance = CONF_IMPEDANCE in config
    if version == 1 and has_impedance:
        raise cv.Invalid(
            "Impedance is only supported for Xiaomi Scale 2 (XMTZC02HM, XMTZC05HM)."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiMiscale),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_VERSION, default=1): cv.int_range(min=1, max=2),
            cv.Optional(CONF_WEIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOGRAM,
                icon=ICON_SCALE_BATHROOM,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_IMPEDANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_OHM,
                icon=ICON_OMEGA,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),

    validate_sensor_match_version
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_version(config[CONF_VERSION]))

    if CONF_WEIGHT in config:
        sens = await sensor.new_sensor(config[CONF_WEIGHT])
        cg.add(var.set_weight(sens))
    if CONF_IMPEDANCE in config:
        sens = await sensor.new_sensor(config[CONF_IMPEDANCE])
        cg.add(var.set_impedance(sens))

