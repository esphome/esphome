import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # No host camera platform exists to emit USE_CAMERA; define it here so
    # the iterator CAMERA state compiles into the test binary.
    async def to_code_testing(config):
        cg.add_define("USE_CAMERA")

    manifest.to_code = to_code_testing
