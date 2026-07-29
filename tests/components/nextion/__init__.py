from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # nextion.h includes esphome/components/display/display.h, so the display
    # component must be in the include path even when no display: config is
    # present (the unit test build only resolves explicit dependencies).
    existing = list(manifest.dependencies or [])
    if "display" not in existing:
        manifest.dependencies = existing + ["display"]