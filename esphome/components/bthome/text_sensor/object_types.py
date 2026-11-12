from ..bthome import BTHomeObjectType, object_types_by_name

TEXT_SENSOR_OBJECT_TYPES = object_types_by_name(
    BTHomeObjectType(object_id=0x53, name="TEXT"),
    BTHomeObjectType(object_id=0x54, name="RAW"),
)
