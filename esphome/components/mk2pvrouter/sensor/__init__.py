from dataclasses import dataclass
from typing import Any

from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_STATE_CLASS,
    CONF_TAG,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)
from esphome.types import ConfigType

from .. import (
    CONF_MK2PVROUTER_ID,
    DUMP_PUBLISHER,
    DUMP_TAG,
    MK2PVROUTER_CHILD_SCHEMA,
    TAG,
    VAL,
    VALUE_PUBLISHER,
    mk2pvrouter_ns,
)


@dataclass(frozen=True)
class TagKind:
    """Sensor defaults for one kind of Mk2PVRouter output."""

    unit_of_measurement: str
    device_class: str
    state_class: str
    accuracy_decimals: int
    # The device sends the value * 100; publish_sensor_value() corrects it.
    scale_centi: bool = False

    def defaults(self) -> dict[str, Any]:
        return {
            CONF_UNIT_OF_MEASUREMENT: self.unit_of_measurement,
            CONF_DEVICE_CLASS: self.device_class,
            CONF_STATE_CLASS: self.state_class,
            CONF_ACCURACY_DECIMALS: self.accuracy_decimals,
        }


POWER = TagKind(UNIT_WATT, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT, 0)
VOLTAGE = TagKind(
    UNIT_VOLT, DEVICE_CLASS_VOLTAGE, STATE_CLASS_MEASUREMENT, 2, scale_centi=True
)
ENERGY = TagKind(UNIT_WATT_HOURS, DEVICE_CLASS_ENERGY, STATE_CLASS_TOTAL_INCREASING, 0)
TEMPERATURE = TagKind(
    UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, scale_centi=True
)
RELAY_STATE = TagKind(UNIT_EMPTY, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE, 0)
DIVERSION_RATE = TagKind(UNIT_PERCENT, DEVICE_CLASS_EMPTY, STATE_CLASS_MEASUREMENT, 0)

# Keyed by (letter, indexed). Per the Mk2PVRouter firmware protocol, D and R mean different
# things bare and indexed: D is diverted power (W) but D1, D2, ... are diversion rates (%);
# R is mean relay power (W) but R1, R2, ... are relay states. T is always indexed, E never.
TAG_KINDS = {
    ("P", False): POWER,
    ("P", True): POWER,
    ("D", False): POWER,
    ("D", True): DIVERSION_RATE,
    ("V", False): VOLTAGE,
    ("V", True): VOLTAGE,
    ("E", False): ENERGY,
    ("T", True): TEMPERATURE,
    ("R", False): POWER,
    ("R", True): RELAY_STATE,
}


def tag_kind(tag: str) -> TagKind | None:
    """The kind of a tag such as P, V1 or R10, or None for a tag that is not a known output."""
    tag = tag.upper()
    index = tag[1:]
    if index and not index.isdigit():
        return None
    return TAG_KINDS.get((tag[:1], bool(index)))


def _inject_tag_defaults(config: ConfigType) -> ConfigType:
    """Fill in the tag's defaults for keys the user did not set; the sensor schema validates them."""
    if (
        isinstance(config, dict)
        and isinstance(config.get(CONF_TAG), str)
        and (kind := tag_kind(config[CONF_TAG])) is not None
    ):
        config = {**kind.defaults(), **config}
    return config


CONFIG_SCHEMA = cv.All(
    _inject_tag_defaults,
    sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(MK2PVROUTER_CHILD_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    tag = config[CONF_TAG]
    kind = tag_kind(tag)
    var = await sensor.new_sensor(config)
    mk2pvrouter = await cg.get_variable(config[CONF_MK2PVROUTER_ID])
    scale_centi = kind is not None and kind.scale_centi
    VALUE_PUBLISHER.add(
        mk2pvrouter,
        mk2pvrouter_ns.publish_sensor_value(
            parent_ref(var), tag, scale_centi, TAG, VAL
        ),
    )
    DUMP_PUBLISHER.add(
        mk2pvrouter, mk2pvrouter_ns.dump_sensor(DUMP_TAG, parent_ref(var), tag)
    )
