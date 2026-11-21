"""The reader's math is header-only, so its unit test needs no camera or decoder."""

from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    """Drop the camera dependencies, which cannot build on the host."""
    manifest.dependencies = []
