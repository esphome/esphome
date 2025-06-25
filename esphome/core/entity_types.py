import esphome.codegen as cg
from esphome.const import CONF_SENSOR, CONF_SWITCH

entity_types = cg.esphome_ns.enum("EntityType", is_class=True)

ENTITY_TYPES = {
    CONF_SWITCH: entity_types.SWITCH,
    CONF_SENSOR: entity_types.SENSOR,
}
