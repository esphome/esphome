import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The column walk in Image::draw() is only built with PSRAM; define it so
    # the rotated benchmark exercises that loop order on the host.
    manifest.enable_codegen()
    original_to_code = manifest.to_code

    async def to_code(config):
        await original_to_code(config)
        cg.add_define("USE_PSRAM")

    if hasattr(original_to_code, "priority"):
        to_code.priority = original_to_code.priority
    manifest.to_code = to_code
