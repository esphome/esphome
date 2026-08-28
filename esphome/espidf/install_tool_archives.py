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

from _tool_resolution import archive_name, init_idf_tools, iter_tool_downloads
from idf_tools import ToolBinaryError, g

from esphome.helpers import rmtree


def collect_pending(
    targets_csv: str, tool_specs: list[str]
) -> dict[tuple[str, str], object]:
    """The {(name, version): tool} jobs whose verified archive is on disk."""
    dist_path = Path(g.idf_tools_path) / "dist"

    def on_broken(name: str, e: ToolBinaryError) -> bool:
        # Repairing a broken installed binary is the installer's job
        print(f"leaving broken {name} to the installer: {e}", file=sys.stderr)
        return False

    pending: dict[tuple[str, str], object] = {}
    for tool, name, version, download in iter_tool_downloads(
        targets_csv, tool_specs, on_broken
    ):
        # An archive at its final name was sha256-verified by the prefetch
        if (name, version) in pending or not (
            dist_path / archive_name(download)
        ).is_file():
            continue
        pending[(name, version)] = tool
    return pending


def install_one(tool: object, name: str, version: str) -> bool | None:
    """True on success, False on a cleaned-up failure, None when the torn
    dest dir survived and could fool the installer's binary probe."""
    try:
        tool.install(version)
    # check_binary_valid exits via SystemExit; the installer redoes failures
    except (Exception, SystemExit) as e:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        print(
            f"pre-extracting {name}@{version} failed, leaving it to the installer: {e}",
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
            return None
        return False
    return True


def main() -> None:
    _script, idf_framework_root, targets_csv, workers_str, *tool_specs = sys.argv
    init_idf_tools(idf_framework_root)
    pending = collect_pending(targets_csv, tool_specs)
    if len(pending) < 2:
        # Nothing to parallelize; the installer keeps its normal output
        return
    workers = min(int(workers_str), len(pending))
    print(
        f"Extracting {len(pending)} ESP-IDF tool archive(s) with "
        f"{workers} worker(s): "
        + ", ".join(f"{name}@{version}" for name, version in pending),
        flush=True,
    )
    with ThreadPoolExecutor(max_workers=workers) as ex:
        futures = [
            ex.submit(install_one, tool, name, version)
            for (name, version), tool in pending.items()
        ]
        try:
            results = [future.result() for future in futures]
        except BaseException:  # pragma: no cover
            # Ctrl-C: drop queued extractions; in-flight ones finish whole
            ex.shutdown(wait=True, cancel_futures=True)
            raise
    # A survivor could fool the installer; every job failing is systematic.
    # Either way a nonzero exit makes the caller warn
    failed = sum(result is not True for result in results)
    if failed:
        print(f"{failed} of {len(results)} pre-extractions failed", file=sys.stderr)
        if None in results or failed == len(results):
            sys.exit(1)


main()
