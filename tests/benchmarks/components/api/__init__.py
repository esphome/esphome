import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Add USE_API_PLAINTEXT so frame helper headers compile.
    # We cannot use enable_codegen() because the full api to_code
    # brings in socket/network setup that fails in benchmark builds.
    async def to_code(config):
        cg.add_define("USE_API_PLAINTEXT")

    manifest.to_code = to_code
