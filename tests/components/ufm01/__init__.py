import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Host unit tests compile ufm01 directly without the full YAML feature set.
    # Emit optional-feature defines so tests exercise the same code paths as
    # fully configured builds.
    async def to_code_testing(config):
        cg.add_define("USE_UFM01_METER_ID")
        cg.add_define("USE_UFM01_SOFTWARE_VERSION")
        cg.add_define("USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION")

    manifest.to_code = to_code_testing
