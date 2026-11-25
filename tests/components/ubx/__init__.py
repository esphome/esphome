from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # ubx declares uart on its gnss platform, which the harness does not walk
    manifest.dependencies = manifest.dependencies + ["uart", "gnss"]
