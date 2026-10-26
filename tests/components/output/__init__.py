import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Builds the min/max power scaling so the Apply output tests cover it
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_OUTPUT_FLOAT_POWER_SCALING")

    manifest.to_code = to_code_testing
