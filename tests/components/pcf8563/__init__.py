from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The driver lives in the time platform, so pull in what it builds against.
    manifest.dependencies = manifest.dependencies + ["i2c", "time"]
