import esphome.codegen as cg
from esphome.components.display import DisplayBuffer
from . import display_buffer_ns

DISPLAY_BUFFER = display_buffer_ns.class_(
    "DisplayBufferImpl", cg.PollingComponent, DisplayBuffer
)

IS_PLATFORM_COMPONENT = True
