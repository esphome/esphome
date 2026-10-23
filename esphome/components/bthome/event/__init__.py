import esphome.codegen as cg
from esphome.components import event
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_INDEX,
    CONF_SOURCE_ID,
    CONF_TYPE,
    DEVICE_CLASS_BUTTON,
)
from esphome.types import ConfigType

from .. import BTHomeRemoteObject, RemoteDevice, add_handler, client_ns
from ..bthome import (
    BTHomeObjectType,
    bthome_object_type_validator,
    bthome_object_types,
    object_types_by_name,
)

CODEOWNERS = ["@jpeletier", "@Bascht74"]

DEPENDENCIES = ["bthome", "event"]

BTHomeEvent = client_ns.class_("BTHomeEvent", BTHomeRemoteObject, event.Event)

EVENT_OBJECT_TYPES = object_types_by_name(
    BTHomeObjectType(
        object_id=0x3A, name="BUTTON_EVENT", device_class=DEVICE_CLASS_BUTTON
    ),
    BTHomeObjectType(object_id=0x3C, name="DIMMER_EVENT"),
)
EVENT_OBJECT_ALIASES = {"BUTTON": "BUTTON_EVENT", "DIMMER": "DIMMER_EVENT"}

# The order is the one BTHomeEvent::process_object maps the event codes to.
EVENT_TYPES = {
    "BUTTON_EVENT": [
        "press",
        "double_press",
        "triple_press",
        "long_press",
        "long_double_press",
        "long_triple_press",
        "hold_press",
    ],
    "DIMMER_EVENT": ["rotate_left", "rotate_right"],
}

# A legacy advertisement has room for about ten one-byte button objects.
MAX_INDEX = 8


def _apply_object_type_defaults(config: ConfigType) -> ConfigType:
    obj = EVENT_OBJECT_TYPES[config[CONF_TYPE]]
    config = dict(config)
    if CONF_DEVICE_CLASS not in config and obj.device_class is not None:
        config[CONF_DEVICE_CLASS] = obj.device_class
    return config


CONFIG_SCHEMA = cv.All(
    event.event_schema(BTHomeEvent).extend(
        {
            cv.Required(CONF_SOURCE_ID): cv.use_id(RemoteDevice),
            cv.Required(CONF_TYPE): bthome_object_type_validator(
                EVENT_OBJECT_TYPES, EVENT_OBJECT_ALIASES
            ),
            cv.Optional(CONF_INDEX, default=1): cv.int_range(min=1, max=MAX_INDEX),
        }
    ),
    _apply_object_type_defaults,
)


async def to_code(config: ConfigType) -> None:
    object_type_key = config[CONF_TYPE]
    var = await event.new_event(config, event_types=EVENT_TYPES[object_type_key])
    cg.add(var.set_object_type(getattr(bthome_object_types, object_type_key)))
    cg.add(var.set_index(config[CONF_INDEX] - 1))
    add_handler(
        var,
        config[CONF_SOURCE_ID],
        EVENT_OBJECT_TYPES[object_type_key].object_id,
    )
