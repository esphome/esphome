import esphome.codegen as cg
import esphome.config_validation as cv


CODEOWNERS = ["@max246"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True


CONF_HP303B_ID = "hp3030b_id"


hp303b_ns = cg.esphome_ns.namespace("hp303b")
HP303BComponent = hp303b_ns.class_("HP303BComponent", cg.PollingComponent)


HB303B_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HP303BComponent),
    }
).extend(cv.polling_component_schema("60s"))


def CONFIG_SCHEMA(conf):
    if conf:
        raise cv.Invalid(
            "This component has been moved in 1.16, please see the docs for updated "
            "instructions. https://esphome.io/components/binary_sensor/pn532.html"
        )


async def setup_hp303b(var, config):
    await cg.register_component(var, config)
