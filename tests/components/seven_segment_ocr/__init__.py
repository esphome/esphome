import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The component has no YAML configuration, so its sources compile to nothing
    # unless the define is set. Enable it for the unit tests.
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_SEVEN_SEGMENT_OCR")

    manifest.to_code = to_code_testing
