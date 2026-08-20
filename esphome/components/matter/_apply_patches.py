"""Apply the esp-matter source patches this component depends on.

Invoked from ``_patch_hook.cmake`` via ``-DCMAKE_PROJECT_INCLUDE``, which
runs AFTER idf-component-manager's integrity-restore pass but BEFORE ninja
starts compiling — the only configure-time hook that lands there. CMake
re-runs whenever inputs change; component-manager may revert our files on
those re-runs and this script re-patches in the same configure pass, so
ninja never sees an unpatched source. On skip-configure builds (day-to-day
rebuilds) the previous configure's patches remain in place.

All patches are idempotent — each writes a marker string on first
application and short-circuits on subsequent runs.

Patches:
  (PATCH1 removed 2026-08 — replaced by External Platform. The old patch
  overwrote NetworkCommissioningDriver_Ethernet.cpp with an empty stub so
  ESPEthernetDriver::Init could be provided by matter_ethernet_stub.cpp
  without a multiple-definition link error. The new mechanism sets
  CONFIG_CHIP_ENABLE_EXTERNAL_PLATFORM=y and points
  CONFIG_CHIP_EXTERNAL_PLATFORM_DIR at
  components/matter/external_platform/, whose external_platform.cmake
  excludes the upstream Ethernet driver from the esp_matter component
  entirely. Same effect, no source edit.)
  2. esp_matter_core.cpp — comment out the ``InitWiFiStack()`` call inside
     ``esp_matter::start()``. On Wi-Fi builds this helper calls
     ``esp_wifi_init()``, which fails with ``ESP_ERR_INVALID_STATE`` because
     ESPHome's ``wifi:`` component already inited the driver — the failure
     aborts ``esp_matter::start()``. On Ethernet-only builds the surrounding
     ``#if CHIP_DEVICE_CONFIG_ENABLE_WIFI`` is off and the call was already
     going to be compiled out; the textual patch is still applied so the
     source is uniform across variants. The one thing ``InitWiFiStack``
     also did — ``esp_event_handler_register(WIFI_EVENT, …,
     PlatformManagerImpl::HandleESPSystemEvent, …)`` — is reproduced from
     ``matter_component.cpp::setup()`` when ``USE_WIFI`` is defined, so
     CHIP still sees ``WIFI_EVENT_STA_DISCONNECTED`` / ``WIFI_EVENT_SCAN_DONE``.
  3. Every ``*_integration.cpp`` (or ``clusters/*/integration.cpp``) that
     declares
     ``std::unordered_map<EndpointId, LazyRegisteredServerCluster<...>> gServers``
     is rewritten to ``std::map``. std::map is a tree, so insertion NEVER
     invalidates pointers to existing elements (C++ spec guarantee). Fixes
     a rehash bug where CHIP's cluster registry holds pointers into the
     map that go dangling when ``unordered_map`` grows past its load-factor
     threshold. On a bridge with 76 endpoints (root + aggregator + 74
     bridged entities) this reproducibly corrupted
     ``DefaultServerCluster::Startup`` mid-boot.
  (PATCH4 removed 2026-08 — the DnssdImpl.cpp exclusion never fires under
  External Platform. Our external_platform.cmake mirrors the else()-branch
  of esp_matter's CMakeLists but simply does not add the
  ``!WIFI_STATION && !WIFI_AP`` exclusion, so ESP32DnssdImpl.cpp is always
  compiled and the EspDnssd* symbols stay linkable on Ethernet-only builds.)
  (PATCH5 removed 2026-08 — the SEC_CERT_DAC_PROVIDER duplicate declaration
  warnings from chip/Kconfig are purely cosmetic; the two "Multiple
  Symbol/Choice Definitions" lines in the configure log are the only
  observable side effect. Dropping the patch cuts one more source of
  upstream-drift breakage in exchange for those warnings.)
  (PATCH6 removed 2026-08 — CONFIG_DISABLE_IPV4 defaults to n in our
  sdkconfig and no test/yaml opts in, so the FATAL_ERROR was defensive
  against a code path never taken. Advanced users who want CHIP compiled
  IPv6-only should pass -DCHIP_DEVICE_CONFIG_ENABLE_IPV4=false via build
  flags instead of flipping CONFIG_DISABLE_IPV4 — same effect, no CMake
  arm involved.)

Errors: PATCH2 is file-specific and load-bearing. If their target file
is missing, or the marker string the patch anchors on is not present in the
current esp-matter release, the script exits non-zero — the CMake hook
surfaces that as a warning and downstream compilation will still fail hard.
Silent-skip on those was masking upstream drift where our patch was quietly
no-op'd but the code was actually broken. PATCH3 iterates across many
integration files; each file may or may not carry the buggy pattern
(``LazyRegisteredServerCluster<…> gServers``), so per-file skips are
legitimate — but if the sweep patches ZERO files, that's fatal too
(esp-matter 1.5+ always has some).
"""

from pathlib import Path
import sys


class PatchError(Exception):
    """Raised when a required patch cannot be applied.

    Marker string not found in the target file, target file missing, or
    the PATCH3 sweep found no matching files — all mean the upstream we
    thought we were patching has drifted, and continuing would produce a
    silently-broken binary.
    """


PATCH2_OLD = (
    "VerifyOrReturnError(chip::DeviceLayer::Internal::ESP32Utils::InitWiFiStack() "
    '== CHIP_NO_ERROR, ESP_FAIL, ESP_LOGE(TAG, "Error initializing Wi-Fi stack"));'
)
PATCH2_NEW = (
    "// ESPHOME-MATTER-PATCH-WIFI: InitWiFiStack skipped — ESPHome owns "
    "esp_wifi_init; WIFI_EVENT handler re-registered from matter_component.cpp"
)

PATCH3_MARKER = "// ESPHOME-MATTER-PATCH-MAP"

# Two substitutions per integration file: swap the include and the type
# alias. std::unordered_map guarantees O(1) average lookup at the price of
# rehashing on insert — which invalidates pointers CHIP holds into the map.
# std::map is a tree (O(log N) lookup) whose iterators/references stay valid
# through all inserts of distinct keys (§26.6.4.4 [container.reqmts]).
PATCH3_INCLUDE_OLD = "#include <unordered_map>"
PATCH3_INCLUDE_NEW = f"#include <map>  {PATCH3_MARKER}"
PATCH3_TYPE_OLD = "std::unordered_map"
PATCH3_TYPE_NEW = "std::map"


def _apply_std_map_swap(path: Path) -> str:
    """Swap `std::unordered_map` → `std::map` in a cluster integration file.

    Only touches files that actually declare a
      std::unordered_map<EndpointId, LazyRegisteredServerCluster<...>> gServers;
    global — the exact pattern that hits the rehash-invalidates-pointers bug.
    Files with unordered_map used for unrelated things (or none at all) are
    skipped. Idempotent via the PATCH3_MARKER comment inserted at the top.

    Some integration files (descriptor_integration.cpp, etc.) get
    `std::unordered_map` from a transitive header include chain rather than
    a direct `#include <unordered_map>`. Handle both cases: if the direct
    include is present, swap it; otherwise insert a fresh `#include <map>`
    below the file's last existing include.

    Per-file misses ARE legitimate here (many integration files don't carry
    the buggy pattern), so this stays tolerant. The aggregate check "at
    least one file was patched" runs in ``apply_patches`` below.
    """
    if not path.exists():
        return f"skip (missing): {path.name}"
    content = path.read_text(encoding="utf-8")
    # Accept the pre-rename marker so a file patched by an older version of
    # this script (before PATCHED-BY-UNISEC-MATTER → ESPHOME-MATTER-PATCH)
    # is not re-processed and left with a duplicate `#include <map>`.
    _LEGACY_PATCH3_MARKER = "// PATCHED-BY-UNISEC-MATTER-MAP"
    if PATCH3_MARKER in content or _LEGACY_PATCH3_MARKER in content:
        return f"noop (already patched): {path.name}"
    if "LazyRegisteredServerCluster" not in content or "gServers" not in content:
        return f"skip (no buggy pattern): {path.name}"

    # Remove the earlier UserLabel-specific reserve() patch if it lingers from
    # a prior build. std::map has no bucket_count/reserve — leaving those in
    # yields a compile error.
    old_marker = "// ESPHOME-MATTER-PATCH-USERLABEL"
    if old_marker in content:
        cleaned_lines = []
        skip_lines = 0
        for line in content.splitlines(keepends=True):
            if skip_lines > 0:
                skip_lines -= 1
                continue
            if old_marker in line:
                # Drop this comment line and the two follow-up lines that
                # make up the original 4-line block (2 comment lines + 1 if).
                skip_lines = 3
                continue
            cleaned_lines.append(line)
        content = "".join(cleaned_lines)

    # Prefer swapping the direct include if it's there; otherwise inject a
    # fresh `#include <map>` right after the last existing include line.
    if PATCH3_INCLUDE_OLD in content:
        patched = content.replace(PATCH3_INCLUDE_OLD, PATCH3_INCLUDE_NEW, 1)
    else:
        lines = content.splitlines(keepends=True)
        last_include_idx = -1
        for i, line in enumerate(lines):
            if line.startswith("#include"):
                last_include_idx = i
        if last_include_idx < 0:
            # Reached here only when the LazyRegisteredServerCluster/gServers
            # pattern IS present (checked above) — returning a silent skip
            # would let the unpatched std::unordered_map ship and reproduce
            # the rehash/dangling-pointer bug this patch exists to fix.
            raise PatchError(
                f"PATCH3: {path.name} declares gServers as unordered_map but "
                f"has no #include line to anchor the map header on — upstream "
                f"file shape changed, update _apply_patches.py"
            )
        lines.insert(last_include_idx + 1, f"{PATCH3_INCLUDE_NEW}\n")
        patched = "".join(lines)

    patched = patched.replace(PATCH3_TYPE_OLD, PATCH3_TYPE_NEW)
    path.write_text(patched, encoding="utf-8")
    return f"map-swap: {path.name}"


def _apply_string_replace(
    path: Path, marker: str, old: str, new: str, legacy_markers: tuple = ()
) -> str:
    if not path.exists():
        raise PatchError(
            f"required patch target missing: {path} — esp-matter version "
            f"drift? update _apply_patches.py"
        )
    content = path.read_text(encoding="utf-8")
    if marker in content:
        return f"noop (already patched): {path.name}"
    # Recognise markers from an earlier version of this script that patched
    # the same anchor. The anchor string was already replaced by that build,
    # so a naive re-run would find neither the new marker nor the anchor and
    # fail with "esp-matter release changed" even though the file is fine.
    # We treat legacy markers as "already patched" and move on.
    for legacy in legacy_markers:
        if legacy in content:
            return f"noop (legacy patch marker present): {path.name}"
    if old not in content:
        # Truncate the expected substring for logging — the full text can be
        # multi-line and swamp CMake's output.
        preview = " ".join(old.split())[:160]
        raise PatchError(
            f"expected upstream marker string not found in {path.name} — "
            f"esp-matter release changed. Update the patch definition in "
            f"_apply_patches.py.\n  expected (first 160 chars): {preview}"
        )
    path.write_text(content.replace(old, new), encoding="utf-8")
    return f"replace: {path.name}"


def apply_patches(matter_dir: Path) -> list:
    """Apply all esp-matter source patches under ``matter_dir``.

    Returns a list of one-line result strings suitable for logging. Raises
    :class:`PatchError` if a required patch cannot be applied — the CLI
    entrypoint (``main``) turns that into a non-zero exit so the CMake hook
    surfaces the failure. Callers that want to keep going on failure must
    catch it themselves.
    """
    results = []

    results.append(
        _apply_string_replace(
            matter_dir / "components" / "esp_matter" / "esp_matter_core.cpp",
            "ESPHOME-MATTER-PATCH-WIFI",
            PATCH2_OLD,
            PATCH2_NEW,
            legacy_markers=("PATCHED-BY-UNISEC-MATTER-WIFI",),
        )
    )

    # Patch 3: every integration.cpp under data_model_provider/clusters/
    # that has the unordered_map<EndpointId, LazyRegisteredServerCluster<>> gServers
    # pattern gets its map type swapped to std::map.
    #
    # esp-matter 1.6.0 reorganised the cluster integrations from
    # `clusters/<name>_integration.cpp` into `clusters/<name>/integration.cpp`.
    # Use a recursive glob so we hit both layouts — _apply_std_map_swap is a
    # no-op on files that don't match the pattern, so the older sibling files
    # (if any survive in some future intermediate layout) stay safe.
    clusters_dir = (
        matter_dir / "components" / "esp_matter" / "data_model_provider" / "clusters"
    )
    if not clusters_dir.is_dir():
        raise PatchError(
            f"PATCH3: cluster integrations dir missing: {clusters_dir} — "
            f"esp-matter reorganised its data_model_provider layout, "
            f"update the glob in _apply_patches.py"
        )
    candidates = set(clusters_dir.glob("*_integration.cpp"))
    candidates.update(clusters_dir.glob("**/integration.cpp"))
    if not candidates:
        raise PatchError(f"PATCH3: no integration.cpp candidates under {clusters_dir}")
    swap_count = 0
    noop_count = 0
    for path in sorted(candidates):
        result = _apply_std_map_swap(path)
        results.append(result)
        if result.startswith("map-swap"):
            swap_count += 1
        elif result.startswith("noop"):
            noop_count += 1
    # After a full sweep at least one file should have carried the buggy
    # pattern — every esp-matter 1.5+ release has ~40 of them. If we
    # patched zero AND noop'd zero, we're patching thin air.
    if swap_count == 0 and noop_count == 0:
        raise PatchError(
            f"PATCH3: swept {len(candidates)} candidate files but found "
            f"no LazyRegisteredServerCluster<…> gServers pattern to swap. "
            f"esp-matter refactored the cluster integration layout — "
            f"update the pattern check in _apply_std_map_swap."
        )
    results.append(
        f"map-swap summary: patched={swap_count} already-patched={noop_count} "
        f"scanned={len(candidates)}"
    )

    return results


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: _apply_patches.py <esp_matter_dir>", file=sys.stderr)
        return 2
    raw = sys.argv[1].strip()
    if not raw:
        print("_apply_patches.py: empty esp_matter_dir arg", file=sys.stderr)
        return 3
    matter_dir = Path(raw)
    if not matter_dir.is_dir():
        # esp-matter not yet downloaded — component-manager normally populates
        # it before this hook runs, but on a cold configure the two can race.
        # Return a distinct code so the CMake hook treats it as tolerable
        # (skip with STATUS) rather than confusing it with a real
        # patch-application failure. Any other non-zero code is fatal.
        print(
            f"_apply_patches.py: esp_matter dir not present yet: "
            f"{matter_dir} — skipping (will retry next configure)",
            file=sys.stderr,
        )
        return 20

    try:
        results = apply_patches(matter_dir)
    except PatchError as e:
        print(f"matter-patches: FATAL: {e}", file=sys.stderr)
        return 4
    for r in results:
        print(f"matter-patches: {r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
