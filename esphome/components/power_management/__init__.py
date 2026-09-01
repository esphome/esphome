import esphome.codegen as cg

IS_PLATFORM_COMPONENT = True
CODEOWNERS = ["@rwrozelle"]

power_management_ns = cg.esphome_ns.namespace("power_management")
PowerManagementComponent = power_management_ns.class_(
    "PowerManagementComponent", cg.Component
)


async def register_power_management(var, config):
    await cg.register_component(var, config)
