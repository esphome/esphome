"""Extract prefetched ESP-IDF tool archives in parallel.

Run via ``python <this file> <idf_framework_root> <targets-csv> <workers>
<tool-spec>...``. PYTHONPATH must include ``<idf_framework_root>/tools`` so
``idf_tools`` is importable, and IDF_TOOLS_PATH must be set.

``idf_tools.py install`` unpacks one archive at a time; this extracts every
tool whose verified archive the prefetch already placed in
``<IDF_TOOLS_PATH>/dist``, several at once, using idf_tools' own
``IDFTool.install()`` so unpacking, container-dir stripping, and the binary
check match the sequential installer exactly. That installer still runs
afterwards as the authority: it skips the tools installed here and redoes
anything this pass failed on, so per-tool failures only warn on stderr.
"""

# pylint: disable=import-error  # idf_tools is on PYTHONPATH at runtime only

from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import sys

from idf_tools import (
    CURRENT_PLATFORM,
    TOOLS_FILE,
    IDFEnv,
    ToolBinaryError,
    add_and_check_targets,
    expand_tools_arg,
    g,
    load_tools_info,
)


def collect_pending() -> list[tuple[object, str, str]]:
    """The (tool, name, version) jobs whose verified archive is on disk."""
    g.idf_path = sys.argv[1]
    g.idf_tools_path = os.environ.get("IDF_TOOLS_PATH")
    g.tools_json = str(Path(g.idf_path) / TOOLS_FILE)

    targets = add_and_check_targets(IDFEnv.get_idf_env(), sys.argv[2])
    tools_info = load_tools_info()
    dist_path = Path(g.idf_tools_path) / "dist"
    pending: list[tuple[object, str, str]] = []
    seen: set[tuple[str, str]] = set()

    for name in expand_tools_arg(sys.argv[4:], tools_info, targets):
        if "@" in name:
            name, version = name.split("@", 1)
        else:
            version = None
        tool = tools_info.get(name)
        if tool is None or not tool.compatible_with_platform():
            continue
        version = version or tool.get_recommended_version()
        if version is None:
            continue
        try:
            tool.find_installed_versions()
        except ToolBinaryError as e:
            # Repairing a broken installed binary is the installer's job
            print(f"leaving broken {name} to the installer: {e}", file=sys.stderr)
            continue
        if version in tool.versions_installed or version not in tool.versions:
            continue
        download = tool.versions[version].get_download_for_platform(CURRENT_PLATFORM)
        if download is None:
            continue
        # An archive at its final name was sha256-verified by the prefetch
        archive = dist_path / (download.rename_dist or Path(download.url).name)
        if not archive.is_file() or (name, version) in seen:
            continue
        seen.add((name, version))
        pending.append((tool, name, version))
    return pending


def install_one(job: tuple[object, str, str]) -> None:
    tool, name, version = job
    try:
        tool.install(version)
    # check_binary_valid exits via SystemExit; the installer redoes failures
    except (Exception, SystemExit) as e:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        print(
            f"pre-extracting {name}@{version} failed, leaving it to the installer: {e}",
            file=sys.stderr,
        )


def main() -> None:
    pending = collect_pending()
    if len(pending) < 2:
        # Nothing to parallelize; the installer keeps its normal output
        return
    workers = min(int(sys.argv[3]), len(pending))
    print(
        f"Extracting {len(pending)} ESP-IDF tool archive(s) with "
        f"{workers} worker(s): "
        + ", ".join(f"{name}@{version}" for _, name, version in pending),
        flush=True,
    )
    with ThreadPoolExecutor(max_workers=workers) as ex:
        list(ex.map(install_one, pending))


main()
