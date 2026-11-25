import esphome.codegen as cg

CODEOWNERS = ["@DT-art1", "@bdraco"]
DOMAIN = "camera"

camera_ns = cg.esphome_ns.namespace("camera")

PixelFormat = camera_ns.enum("PixelFormat")

Decoder = camera_ns.class_("Decoder")
