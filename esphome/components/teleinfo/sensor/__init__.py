import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ICON,
    CONF_ID,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_REACTIVE_ENERGY,
    DEVICE_CLASS_VOLTAGE,
    ICON_FLASH,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_KILOVOLT_AMPS,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE_HOURS,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)
from esphome.types import ConfigType

from .. import CONF_TAG_NAME, CONF_TELEINFO_ID, TELEINFO_LISTENER_SCHEMA, teleinfo_ns

TeleInfoSensor = teleinfo_ns.class_("TeleInfoSensor", sensor.Sensor, cg.Component)


def _preset(unit: str, device_class: str, state_class: str) -> dict[str, str]:
    return {
        CONF_UNIT_OF_MEASUREMENT: unit,
        CONF_DEVICE_CLASS: device_class,
        CONF_STATE_CLASS: state_class,
    }


_ENERGY = _preset(UNIT_WATT_HOURS, DEVICE_CLASS_ENERGY, STATE_CLASS_TOTAL_INCREASING)
_REACTIVE_ENERGY = _preset(
    UNIT_VOLT_AMPS_REACTIVE_HOURS,
    DEVICE_CLASS_REACTIVE_ENERGY,
    STATE_CLASS_TOTAL_INCREASING,
)
_CURRENT = _preset(UNIT_AMPERE, DEVICE_CLASS_CURRENT, STATE_CLASS_MEASUREMENT)
_VOLTAGE = _preset(UNIT_VOLT, DEVICE_CLASS_VOLTAGE, STATE_CLASS_MEASUREMENT)
_APPARENT_POWER_VA = _preset(
    UNIT_VOLT_AMPS, DEVICE_CLASS_APPARENT_POWER, STATE_CLASS_MEASUREMENT
)
_APPARENT_POWER_KVA = _preset(
    UNIT_KILOVOLT_AMPS, DEVICE_CLASS_APPARENT_POWER, STATE_CLASS_MEASUREMENT
)
_POWER = _preset(UNIT_WATT, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT)

# Legacy defaults, kept for tags without a known preset
_LEGACY_DEFAULT = {**_ENERGY, CONF_ICON: ICON_FLASH}

# Presets by tag prefix. No prefix is a prefix of another, so order is irrelevant.
TIC_TAG_CONFIGS = {
    # Standard mode
    "EA": _ENERGY,
    "ER": _REACTIVE_ENERGY,
    "IRMS": _CURRENT,
    "U": _VOLTAGE,
    "SINST": _APPARENT_POWER_VA,
    "SMAX": _APPARENT_POWER_VA,
    "CC": _POWER,
    "PREF": _APPARENT_POWER_KVA,
    "PCOUP": _APPARENT_POWER_KVA,
    # Historical mode
    "BASE": _ENERGY,
    "HCH": _ENERGY,
    "EJP": _ENERGY,
    "BBRH": _ENERGY,
    "IINST": _CURRENT,
    "IMAX": _CURRENT,
    "ISOUSC": _CURRENT,
    "ADPS": _CURRENT,
    "PAPP": _APPARENT_POWER_VA,
    "PMAX": _POWER,
}


def apply_tag_config(config: ConfigType) -> ConfigType:
    """Apply preset configurations based on the tag name.

    Only keys not set by the user are filled in. Unit and device class are
    treated as a pair: if the user sets either, neither is taken from the preset.
    """
    if CONF_TAG_NAME not in config:
        return config

    tag_name = config[CONF_TAG_NAME]
    preset = next(
        (p for prefix, p in TIC_TAG_CONFIGS.items() if tag_name.startswith(prefix)),
        _LEGACY_DEFAULT,
    )

    skip = set()
    if CONF_UNIT_OF_MEASUREMENT in config or CONF_DEVICE_CLASS in config:
        skip = {CONF_UNIT_OF_MEASUREMENT, CONF_DEVICE_CLASS}

    config = dict(config)
    for key, value in preset.items():
        if key not in config and key not in skip:
            config[key] = value
    return config


CONFIG_SCHEMA = cv.All(
    apply_tag_config,
    sensor.sensor_schema(
        TeleInfoSensor,
        accuracy_decimals=0,
    ).extend(TELEINFO_LISTENER_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_TAG_NAME])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    teleinfo = await cg.get_variable(config[CONF_TELEINFO_ID])
    cg.add(teleinfo.register_teleinfo_listener(var))
