from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The driver is a sensor platform, so pull in the bus and the entities it builds against.
    manifest.dependencies = manifest.dependencies + ["i2c", "sensor"]
