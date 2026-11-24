from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # lsm6ds declares i2c on its motion platform, which the harness does not walk
    manifest.dependencies = manifest.dependencies + ["i2c", "motion"]
