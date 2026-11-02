import esphome.codegen as cg
from esphome.loader import FileResource
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The output button helper is under test; its button platform is not in this build
    manifest.dependencies = manifest.dependencies + ["button"]
    manifest.add_resources(
        FileResource("esphome.components.output", "button/output_button.h"),
        FileResource("esphome.components.output", "button/output_button.cpp"),
    )

    # Builds the min/max power scaling so the Apply output tests cover it
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_OUTPUT_FLOAT_POWER_SCALING")

    manifest.to_code = to_code_testing
