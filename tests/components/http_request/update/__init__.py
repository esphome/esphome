import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The entity has no target in a unit test build, so the real to_code cannot run; the
    # define it emits is what compiles the target hooks into the sources under test.
    async def to_code_testing(config):
        cg.add_define("USE_HTTP_REQUEST_UPDATE")

    manifest.to_code = to_code_testing
