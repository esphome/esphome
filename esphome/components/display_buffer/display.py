import esphome.codegen as cg
from . import display_buffer_ns
from esphome.components.display import DisplayBuffer

DISPLAY_BUFFER = display_buffer_ns.class_(
    "DisplayBufferImpl", cg.PollingComponent, DisplayBuffer
)

IS_PLATFORM_COMPONENT = True
