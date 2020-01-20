import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_NAME
from esphome.core import ID

MULTI_CONF = True

gradient_ns = cg.esphome_ns.namespace('gradient')
# Gradient = gradient_ns.class_('Gradient', cg.Component)
Gradient = gradient_ns.class_('Gradient')
GradientPtr = Gradient.operator('ptr')
GradientPoint = gradient_ns.struct('GradientPoint')

CONF_GRADIENT = 'gradient'
CONF_GIMP_GRADIENT = 'gimp'
CONF_RGB_GRADIENT = 'rgb'
CONF_POS = "pos"
CONF_RED = "r"
CONF_GREEN = "g"
CONF_BLUE = "b"
CONF_GRADIENT_DATA = 'gradient_data'

GRADIENT_RGB_SCHEMA = cv.Schema({
    #cv.Required(CONF_POS): cv.zero_to_one_float,
    cv.Optional(CONF_RED, default=0.0): cv.zero_to_one_float,
    cv.Optional(CONF_GREEN, default=0.0): cv.zero_to_one_float,
    cv.Optional(CONF_BLUE, default=0.0): cv.zero_to_one_float
})

CONFIG_SCHEMA = cv.All(cv.Schema({
    # cv.Required(CONF_ID): cv.declare_id(Gradient),
    cv.GenerateID(): cv.declare_id(Gradient),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional(CONF_GIMP_GRADIENT): cv.string,
    cv.Optional(CONF_RGB_GRADIENT): cv.ensure_list(GRADIENT_RGB_SCHEMA),
    cv.GenerateID(CONF_GRADIENT_DATA): cv.declare_id(GradientPoint),
}),#.extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_GIMP_GRADIENT, CONF_RGB_GRADIENT))


def to_code(config):
    #var = cg.new_Pvariable(config[CONF_ID])
    cg.add_global(gradient_ns.using)
    print("creating gradient: %s %s" %(config[CONF_ID], type(config[CONF_ID])))
    var = cg.new_Pvariable(config[CONF_ID])
    #yield cg.register_component(var, config)
    
    segments = []

    if CONF_NAME in config:
        name = config[CONF_NAME]
        # set name

    if CONF_RGB_GRADIENT in config:
        le = len(config[CONF_RGB_GRADIENT])
        if le >= 2:
            segsize = 1.0/(le-1)
        else:
            segsize = 1.0
        pos = 0.0
        lst = list(config[CONF_RGB_GRADIENT])
        for (i, seg) in enumerate(lst):
            # l, m, r, rl, gl, bl, rr, gr, br, fn, space;
            
            segments.append(cg.StructInitializer(
                    GradientPoint,
                    ('l', pos),
                    ('m', pos+(segsize/2.0)),
                    ('r', pos+segsize),
                    ('rl', seg["r"]),
                    ('gl', seg["g"]),
                    ('bl', seg["b"]),
                    ('rr', lst[(i + 1) % le]["r"]),
                    ('gr', lst[(i + 1) % le]["g"]),
                    ('br', lst[(i + 1) % le]["b"]),
                    ('fn', 0),
                    ('space', 0),
            ))
            pos = segsize * i

    if CONF_GIMP_GRADIENT in config:
        from esphome import ggr
        print("----")
        print(config[CONF_GIMP_GRADIENT], type(config[CONF_GIMP_GRADIENT]))
        print("---###5")
        gg = ggr.GimpGradient(content=config[CONF_GIMP_GRADIENT])
        # FIXME: does not work because validator compains first
        # use the gimp gradient name if available and name is default
        #if gg.name and  config[CONF_NAME] == "Gradient":
        #    name = gg.name
        
        #grad = "AdressableGradient_%s" %effect_id
        #grad.type = AddressableGradientEffectColor
        # cg.progmem_array(grad, rhs)
        for seg in gg.segs:
            # l, m, r, rl, gl, bl, rr, gr, br, fn, space;
            segments.append(cg.StructInitializer(
                    GradientPoint,
                    ('l', seg.l),
                    ('m', seg.m),
                    ('r', seg.r),
                    ('rl', seg.rl),
                    ('gl', seg.gl),
                    ('bl', seg.bl),
                    ('rr', seg.rr),
                    ('gr', seg.gr),
                    ('br', seg.br),
                    ('fn', int(seg.fn)),
                    ('space', int(seg.space)),
            ))
    # cg.add(var.set_gradient(segments))
    #grad = "AdressableGradient_%s" %effect_id
    #grad.type = AddressableGradientEffectColor
    # cg.progmem_array(length, segments)
    if len(segments):
        #pg_array = ID("%s_points" % config[CONF_ID], is_declaration=True, type=GradientPoint, is_manual=True)
        print("%s %s" %(config[CONF_ID], type(config[CONF_ID])))
        #pgr = cg.progmem_array(pg_array, segments)
        pgr = cg.progmem_array(config[CONF_GRADIENT_DATA], segments)
        print(pgr)
        cg.add(var.set_gradient(pgr, len(segments)))
    
    yield var
    # pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    # one_wire = cg.new_Pvariable(config[CONF_ONE_WIRE_ID], pin)
    # var = cg.new_Pvariable(config[CONF_ID], one_wire)
    # yield cg.register_component(var, config)
