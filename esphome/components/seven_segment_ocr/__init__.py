"""Seven-segment digit recognition on a grayscale image."""

import esphome.codegen as cg
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "seven_segment_ocr"

seven_segment_ocr_ns = cg.esphome_ns.namespace("seven_segment_ocr")
SevenSegmentOCR = seven_segment_ocr_ns.class_("SevenSegmentOCR")


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_SEVEN_SEGMENT_OCR")
