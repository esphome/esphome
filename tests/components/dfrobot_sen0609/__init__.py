import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # One empty config entry so to_code runs and sets the define that keeps
    # core/line_reader.cpp in the test build.
    manifest.multi_conf = False
    manifest.config_schema = cv.Schema({})

    async def to_code(config: ConfigType) -> None:
        cg.add_define("USE_LINE_READER")

    manifest.to_code = to_code
