from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # nextion.h includes esphome/components/display/display.h and
    # esphome/components/uart/uart.h.  Declare both as test dependencies
    # so their headers are in the include path during C++ unit test builds.
    deps = list(manifest.dependencies or [])
    for dep in ("display", "uart"):
        if dep not in deps:
            deps.append(dep)
    manifest.dependencies = deps
