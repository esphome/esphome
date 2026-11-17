"""Extract prefetched ESP-IDF tool archives in parallel.

Run via ``python <this file> <idf_framework_root> <targets-csv> <workers>
<tool-spec>...`` with idf_tools and the esphome package root on PYTHONPATH
and IDF_TOOLS_PATH set.
Drives idf_tools' own ``IDFTool.install()`` so extraction semantics match
the sequential installer, which still runs afterwards as the authority and
redoes anything this best-effort pass failed on. Archives are trusted from
the prefetch's sha256 verification, not re-hashed here.
"""

# pylint: disable=import-error  # idf_tools is on PYTHONPATH at runtime only

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import sys
from typing import Any

from _tool_resolution import archive_name, init_idf_tools, iter_tool_downloads
from idf_tools import ToolBinaryError, g

from esphome.helpers import rmtree


def collect_pending(
    targets_csv: str, tool_specs: list[str]
) -> dict[tuple[str, str], Any]:
    """The {(name, version): tool} jobs whose verified archive is on disk."""
    dist_path = Path(g.idf_tools_path) / "dist"

    broken: set[str] = set()

    def on_broken(name: str, e: ToolBinaryError) -> None:
        # Repairing a broken installed binary is the installer's job, but
        # the prefetch listed the tool, so it must still claim its filename
        print(f"leaving broken {name} to the installer: {e}", file=sys.stderr)
        broken.add(name)

    pending: dict[tuple[str, str], Any] = {}
    claimed: dict[str, str] = {}
    for tool, name, version, download in iter_tool_downloads(
        targets_csv, tool_specs, on_broken
    ):
        # Mirror the prefetch: an entry it could not verify is never trusted
        if not (download.sha256 and download.size):
            continue
        dist_name = archive_name(download)
        # Mirror the prefetch's dest dedupe: a name claimed by a different
        # sha256 was verified for that entry only, not this one
        if claimed.setdefault(dist_name, download.sha256) != download.sha256:
            print(
                f"leaving {name}@{version} to the installer: {dist_name} "
                "holds a different archive",
                file=sys.stderr,
            )
            continue
        if name in broken:
            continue
        # Trusted as-is: the caller only runs this pass after a prefetch
        # that verified every archive at its final name
        if (dist_path / dist_name).is_file():
            pending[(name, version)] = tool
    return pending


def install_one(tool: Any, name: str, version: str) -> bool | str:
    """True when extracted, False when failed and cleaned, the dest path
    when a torn dir survived cleanup; failures go back to the installer."""
    try:
        tool.install(version)
    # check_binary_valid exits via SystemExit; the installer redoes failures
    except (Exception, SystemExit) as e:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # Name the type: idf_tools' fatal() raises SystemExit(1), which
        # would render as a bare "1"
        print(
            f"pre-extracting {name}@{version} failed, leaving it to the "
            f"installer: {type(e).__name__}: {e}",
            file=sys.stderr,
        )
        # A torn dest dir must not look installed to the installer
        dest = tool.get_path_for_version(version)
        try:
            rmtree(dest)
        except FileNotFoundError:  # pragma: no cover  # failed before mkdir
            pass
        except OSError as cleanup_err:
            print(
                f"could not remove {dest}: {cleanup_err}; the installer may "
                "trust the partial tool dir, delete it manually if the build "
                "fails",
                file=sys.stderr,
            )
            return dest
        return False
    # Per-tool completion keeps the multi-minute unpack phase visibly alive
    print(f"extracted {name}@{version}", flush=True)
    return True


def main() -> None:
    _script, idf_framework_root, targets_csv, workers_str, *tool_specs = sys.argv
    init_idf_tools(idf_framework_root)
    pending = collect_pending(targets_csv, tool_specs)
    if len(pending) < 2:
        # Nothing to parallelize; the installer takes them
        print(
            f"{len(pending)} prefetched tool archive(s); leaving them to the installer",
            flush=True,
        )
        return
    workers = min(int(workers_str), len(pending))
    print(
        f"Extracting {len(pending)} ESP-IDF tool archive(s) with "
        f"{workers} worker(s): "
        + ", ".join(f"{name}@{version}" for name, version in pending),
        flush=True,
    )
    ex = ThreadPoolExecutor(max_workers=workers)
    try:
        futures = [
            ex.submit(install_one, tool, name, version)
            for (name, version), tool in pending.items()
        ]
        results = [future.result() for future in futures]
    finally:
        # Ctrl-C drops queued extractions; in-flight ones finish whole
        ex.shutdown(wait=True, cancel_futures=True)
    if failed := sum(result is not True for result in results):
        # Nonzero exit makes the caller warn; the installer redoes these
        print(f"{failed} of {len(results)} pre-extractions failed", file=sys.stderr)
        # Repeated last so it is not lost in the streamed install output
        for dest in (r for r in results if isinstance(r, str)):
            print(
                f"partial tool dir survived: {dest}; delete it manually if "
                "the build fails",
                file=sys.stderr,
            )
        sys.exit(1)


main()
