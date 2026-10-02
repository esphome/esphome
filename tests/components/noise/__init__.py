import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # to_code must run: it defines USE_NOISE and adds the noise-c library
    # the component sources under test need.
    manifest.enable_codegen()
    real_to_code = manifest.to_code

    async def to_code_testing(config):
        await real_to_code(config)
        cg.add_define("USE_NOISE_SPARE_EPHEMERAL")

    manifest.to_code = to_code_testing
