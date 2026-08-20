from esphome.components.runtime_image import enable_format
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # to_code is suppressed in cpptest builds; formats are normally enabled by
    # process_runtime_image_config(). Enable all formats so the format-switch
    # tests have two decoder types and every retained decoder is under test.
    async def to_code_testing(config: ConfigType) -> None:
        enable_format("BMP")
        enable_format("PNG")
        enable_format("JPEG")

    manifest.to_code = to_code_testing
