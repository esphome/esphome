import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The column walk in Display::draw_pixels_at() is only built with PSRAM;
    # define it so the host test can run both walk orders.
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_PSRAM")

    manifest.to_code = to_code_testing
