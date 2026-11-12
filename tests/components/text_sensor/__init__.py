import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # filter.cpp only compiles with the define the real to_code emits for configured filters
        cg.add_define("USE_TEXT_SENSOR_FILTER")
        cg.add_define("USE_TEXT_SENSOR_FILTER_RESET")

    manifest.to_code = to_code_testing
