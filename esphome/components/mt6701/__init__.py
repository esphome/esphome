import esphome.codegen as cg

CODEOWNERS = ["@slimcdk"]
DOMAIN = "mt6701"

mt6701_ns = cg.esphome_ns.namespace("mt6701")
MT6701Component = mt6701_ns.class_("MT6701Component", cg.Component)

CONF_MT6701_ID = "mt6701_id"
