import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The replay check is compiled only when a sensor enables replay_protection; the
    # unit-test build has no sensors, so emit the define here to put it under test.
    async def to_code_testing(config):
        cg.add_define("USE_BTHOME_MITHERMOMETER_REPLAY_PROTECTION")

    manifest.to_code = to_code_testing
