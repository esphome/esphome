import esphome.codegen as cg
from esphome.components import uart
from esphome.components.ld600x import (
    LD600XComponent,
    ld600x_hub_schema,
    request_model_sizes,
    uart_final_validate,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["ld600x"]
CODEOWNERS = ["@wolph"]
DEPENDENCIES = ["uart"]
DOMAIN = "ld6004"
MULTI_CONF = True

ld6004_ns = cg.esphome_ns.namespace("ld6004")
LD6004Component = ld6004_ns.class_("LD6004Component", LD600XComponent)

CONFIG_SCHEMA = ld600x_hub_schema(LD6004Component)
FINAL_VALIDATE_SCHEMA = uart_final_validate("ld6004")


async def to_code(config: ConfigType) -> None:
    var: MockObj = await uart.new_uart_component(config)
    request_model_sizes(var, max_targets=3, area_kinds=3)
