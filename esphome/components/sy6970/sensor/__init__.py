from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLIAMP,
    UNIT_VOLT,
)
from esphome.types import ConfigType

from .. import CONF_SY6970_ID, DATA_PUBLISHER, REGISTERS, SY6970Component, sy6970_ns

DEPENDENCIES = ["sy6970"]

CONF_VBUS_VOLTAGE = "vbus_voltage"
CONF_SYSTEM_VOLTAGE = "system_voltage"
CONF_CHARGE_CURRENT = "charge_current"
CONF_PRECHARGE_CURRENT = "precharge_current"


def _decode(
    reg: str, step: str, base: str | None = None, shift: int = 0, mask: int = 0x7F
) -> cg.MockObj:
    """``base + ((registers[reg] >> shift) & mask) * step``, from the hub's constants."""
    raw = REGISTERS[getattr(sy6970_ns, reg)]
    if shift:
        raw = raw >> shift
    value = (raw & mask) * getattr(sy6970_ns, step)
    return value if base is None else getattr(sy6970_ns, base) + value


# Voltages in V, currents in mA
STATES = {
    CONF_VBUS_VOLTAGE: _decode(
        "SY6970_REG_VBUS_VOLTAGE", "VBUS_STEP_MV", "VBUS_BASE_MV"
    )
    * 0.001,
    CONF_BATTERY_VOLTAGE: _decode("SY6970_REG_BATV", "VBAT_STEP_MV", "VBAT_BASE_MV")
    * 0.001,
    CONF_SYSTEM_VOLTAGE: _decode(
        "SY6970_REG_VINDPM_STATUS", "VSYS_STEP_MV", "VSYS_BASE_MV"
    )
    * 0.001,
    CONF_CHARGE_CURRENT: _decode(
        "SY6970_REG_CHARGE_CURRENT_MONITOR", "CHG_CURRENT_STEP_MA"
    ),
    CONF_PRECHARGE_CURRENT: _decode(
        "SY6970_REG_PRECHARGE_CURRENT",
        "PRE_CHG_STEP_MA",
        "PRE_CHG_BASE_MA",
        shift=4,
        mask=0x0F,
    ),
}


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SY6970_ID): cv.use_id(SY6970Component),
        cv.Optional(CONF_VBUS_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SYSTEM_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CHARGE_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIAMP,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PRECHARGE_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIAMP,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SY6970_ID])

    sensors = sensor.sub_sensors(config)
    for key, state in STATES.items():
        if var := await sensors(key, None):
            DATA_PUBLISHER.add(hub, parent_ref(var).publish_state(state))
