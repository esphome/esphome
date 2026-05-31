import esphome.codegen as cg

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["esp32"]

ring_buffer_ns = cg.esphome_ns.namespace("ring_buffer")
RingBuffer = ring_buffer_ns.class_("RingBuffer")
