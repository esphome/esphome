from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # A sensor-only platform declares its dependencies on the platform, which the host
    # unit-test build does not walk, so name them here for the build.
    manifest.dependencies = manifest.dependencies + ["i2c", "sensor"]
