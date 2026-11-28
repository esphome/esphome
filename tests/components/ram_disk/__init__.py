from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The driver is a storage platform, so pull in the block device base class.
    manifest.dependencies = manifest.dependencies + ["storage"]
