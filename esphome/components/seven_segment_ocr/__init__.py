"""Seven-segment digit recognition on a grayscale image."""

import esphome.codegen as cg
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "seven_segment_ocr"


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_SEVEN_SEGMENT_OCR")
