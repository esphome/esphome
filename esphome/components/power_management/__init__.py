import esphome.codegen as cg
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

IS_PLATFORM_COMPONENT = True
CODEOWNERS = ["@rwrozelle"]
DOMAIN = "power_management"

power_management_ns = cg.esphome_ns.namespace("power_management")
PowerManagementComponent = power_management_ns.class_(
    "PowerManagementComponent", cg.Component
)


async def register_power_management(var: MockObj, config: ConfigType) -> None:
    await cg.register_component(var, config)
