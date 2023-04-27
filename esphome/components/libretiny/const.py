import esphome.codegen as cg

CONF_LIBRETINY = "libretiny"
KEY_LIBRETINY = "libretiny"
KEY_BOARD = "board"

libretiny_ns = cg.esphome_ns.namespace("libretiny")
LTComponent = libretiny_ns.class_("LTComponent", cg.PollingComponent)
