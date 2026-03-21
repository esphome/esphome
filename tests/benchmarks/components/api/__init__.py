import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride

# Keep in sync with esphome/components/api/__init__.py
NOISE_C_LIB = ("esphome/noise-c", "0.1.11")


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Add defines for frame helper headers without running the full api
    # to_code (which brings in socket/network setup that fails in
    # benchmark builds).
    async def to_code(config):
        cg.add_define("USE_API_PLAINTEXT")
        cg.add_define("USE_API_NOISE")
        cg.add_library(*NOISE_C_LIB)

    manifest.to_code = to_code
