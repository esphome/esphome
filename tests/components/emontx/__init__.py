import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Only the define that keeps core/line_reader.cpp in the test build
    async def to_code(config: ConfigType) -> None:
        cg.add_define("USE_LINE_READER")

    manifest.to_code = to_code
