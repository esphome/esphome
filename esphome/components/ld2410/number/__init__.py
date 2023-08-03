from esphome.components import number
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    CONF_TIMEOUT,
    CONF_THRESHOLD,
)

from .. import ld2410_ns, CONF_LD2410_ID, LD2410Component

DEPENDENCIES = ["ld2410"]
CODEOWNERS = ["@hlyi"]

LD2410Number = ld2410_ns.class_("LD2410Number", number.Number, cg.Component)

LD2410NumType = ld2410_ns.enum("LD2410NumType")

THRESHOLD_TYPE = {
    "move": LD2410NumType.LD2410_THRES_MOVE,
    "still": LD2410NumType.LD2410_THRES_STILL,
}

CONF_GATE_NUM = "gate_num"
CONF_MAXDIST_STILL = "maxdist_still"
CONF_MAXDIST_MOVE = "maxdist_move"

THRES_SCHEMA = cv.All(
    number.number_schema(LD2410Number)
    .extend(
        {
            cv.Required(CONF_GATE_NUM): cv.int_range(min=0, max=8),
            cv.Required(CONF_TYPE): cv.enum(THRESHOLD_TYPE),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

DIST_SCHEMA = cv.All(number.number_schema(LD2410Number).extend(cv.COMPONENT_SCHEMA))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
        cv.Optional(CONF_THRESHOLD): cv.ensure_list(THRES_SCHEMA),
        cv.Optional(CONF_TIMEOUT): DIST_SCHEMA,
        cv.Optional(CONF_MAXDIST_STILL): DIST_SCHEMA,
        cv.Optional(CONF_MAXDIST_MOVE): DIST_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_LD2410_ID])

    if CONF_THRESHOLD in config.keys():
        for dev in config[CONF_THRESHOLD]:
            var = cg.new_Pvariable(dev[CONF_ID], dev[CONF_GATE_NUM], dev[CONF_TYPE])
            await cg.register_component(var, dev)
            await number.register_number(
                var,
                dev,
                min_value=0,
                max_value=100,
                step=1,
            )
            cg.add(var.set_ld2410_parent(paren))
            cg.add(paren.set_number_cb(dev[CONF_GATE_NUM], dev[CONF_TYPE], var))

    if CONF_TIMEOUT in config.keys():
        dev = config[CONF_TIMEOUT]
        var = cg.new_Pvariable(dev[CONF_ID], -1, LD2410NumType.LD2410_TIMEOUT)
        await cg.register_component(var, dev)
        await number.register_number(
            var,
            dev,
            min_value=1,
            max_value=3600,
            step=1,
        )
        cg.add(var.set_ld2410_parent(paren))
        cg.add(paren.set_number_cb(-1, LD2410NumType.LD2410_TIMEOUT, var))

    if CONF_MAXDIST_STILL in config.keys():
        dev = config[CONF_MAXDIST_STILL]
        var = cg.new_Pvariable(dev[CONF_ID], -1, LD2410NumType.LD2410_MAXDIST_STILL)
        await cg.register_component(var, dev)
        await number.register_number(
            var,
            dev,
            min_value=1,
            max_value=8,
            step=1,
        )
        cg.add(var.set_ld2410_parent(paren))
        cg.add(paren.set_number_cb(-1, LD2410NumType.LD2410_MAXDIST_STILL, var))

    if CONF_MAXDIST_MOVE in config.keys():
        dev = config[CONF_MAXDIST_MOVE]
        var = cg.new_Pvariable(dev[CONF_ID], -1, LD2410NumType.LD2410_MAXDIST_MOVE)
        await cg.register_component(var, dev)
        await number.register_number(
            var,
            dev,
            min_value=1,
            max_value=8,
            step=1,
        )
        cg.add(var.set_ld2410_parent(paren))
        cg.add(paren.set_number_cb(-1, LD2410NumType.LD2410_MAXDIST_MOVE, var))
