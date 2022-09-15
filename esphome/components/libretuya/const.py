import esphome.codegen as cg

CONF_LIBRETUYA_ID = "libretuya"
KEY_LIBRETUYA = "libretuya"
KEY_BOARD = "board"

libretuya_ns = cg.esphome_ns.namespace("libretuya")
LTComponent = libretuya_ns.class_("LTComponent", cg.PollingComponent)
