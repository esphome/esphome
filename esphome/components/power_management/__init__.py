import esphome.codegen as cg

CODEOWNERS = ["@silverchris"]
IS_PLATFORM_COMPONENT = True


power_management_ns = cg.esphome_ns.namespace("power_management")


async def to_code(config):
    cg.add_global(power_management_ns.using)
