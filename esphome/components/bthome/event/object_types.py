from esphome import const

from ..bthome import BTHomeObjectType, object_types_by_name

EVENT_OBJECT_TYPES = object_types_by_name(
    BTHomeObjectType(
        object_id=0x3A, name="BUTTON_EVENT", device_class=const.DEVICE_CLASS_BUTTON
    ),
    BTHomeObjectType(object_id=0x3C, name="DIMMER_EVENT"),
)
EVENT_OBJECT_ALIASES = {"BUTTON": "BUTTON_EVENT", "DIMMER": "DIMMER_EVENT"}
