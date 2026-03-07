#!/usr/bin/env python3
"""Post or update a PR comment with memory impact analysis results.

This script creates or updates a GitHub PR comment with memory usage changes.
It uses the GitHub CLI (gh) to manage comments and maintains a single comment
that gets updated on subsequent runs.
"""

from __future__ import annotations

import argparse
import difflib
import json
from pathlib import Path
import re
import subprocess
import sys

from jinja2 import Environment, FileSystemLoader

# Add esphome to path for analyze_memory import
sys.path.insert(0, str(Path(__file__).parent.parent))

# pylint: disable=wrong-import-position

# Comment marker to identify our memory impact comments
COMMENT_MARKER = "<!-- esphome-memory-impact-analysis -->"


def run_gh_command(args: list[str], operation: str) -> subprocess.CompletedProcess:
    """Run a gh CLI command with error handling.

    Args:
        args: Command arguments (including 'gh')
        operation: Description of the operation for error messages

    Returns:
        CompletedProcess result

    Raises:
        subprocess.CalledProcessError: If command fails (with detailed error output)
    """
    try:
        return subprocess.run(
            args,
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError as e:
        print(
            f"ERROR: {operation} failed with exit code {e.returncode}", file=sys.stderr
        )
        print(f"ERROR: Command: {' '.join(args)}", file=sys.stderr)
        print(f"ERROR: stdout: {e.stdout}", file=sys.stderr)
        print(f"ERROR: stderr: {e.stderr}", file=sys.stderr)
        raise


# Thresholds for emoji significance indicators (percentage)
OVERALL_CHANGE_THRESHOLD = 1.0  # Overall RAM/Flash changes
COMPONENT_CHANGE_THRESHOLD = 3.0  # Component breakdown changes

# Display limits for tables
MAX_COMPONENT_BREAKDOWN_ROWS = 20  # Maximum components to show in breakdown table
MAX_CHANGED_SYMBOLS_ROWS = 30  # Maximum changed symbols to show
MAX_NEW_SYMBOLS_ROWS = 15  # Maximum new symbols to show
MAX_REMOVED_SYMBOLS_ROWS = 15  # Maximum removed symbols to show

# Symbol display formatting
SYMBOL_DISPLAY_MAX_LENGTH = 100  # Max length before using <details> tag
SYMBOL_DISPLAY_TRUNCATE_LENGTH = 97  # Length to truncate in summary

# Component change noise threshold
COMPONENT_CHANGE_NOISE_THRESHOLD = 2  # Ignore component changes ≤ this many bytes

# Disassembly diff limits
MAX_DISASM_DIFF_SYMBOLS = 15  # Maximum symbols to show disassembly diffs for
MAX_DISASM_DIFF_LINES = 80  # Maximum lines per disassembly diff
MAX_DISASM_NEW_LINES = 40  # Maximum lines for new/removed symbol disassembly
# Budget: keep total disassembly under ~40KB to stay within GitHub's 65536 char limit
MAX_DISASM_TOTAL_CHARS = 40000
# Skip disassembly analysis entirely if too many symbols changed
MAX_DISASM_TOTAL_CHANGED_SYMBOLS = 50

# Template directory
TEMPLATE_DIR = Path(__file__).parent / "templates"


def load_analysis_json(json_path: str) -> dict | None:
    """Load memory analysis results from JSON file.

    Args:
        json_path: Path to analysis JSON file

    Returns:
        Dictionary with analysis results or None if file doesn't exist/can't be loaded
    """
    json_file = Path(json_path)
    if not json_file.exists():
        print(f"Analysis JSON not found: {json_path}", file=sys.stderr)
        return None

    try:
        with open(json_file, encoding="utf-8") as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError) as e:
        print(f"Failed to load analysis JSON: {e}", file=sys.stderr)
        return None


def format_bytes(bytes_value: int) -> str:
    """Format bytes value with comma separators.

    Args:
        bytes_value: Number of bytes

    Returns:
        Formatted string with comma separators (e.g., "1,234 bytes")
    """
    return f"{bytes_value:,} bytes"


def format_change(before: int, after: int, threshold: float | None = None) -> str:
    """Format memory change with delta and percentage.

    Args:
        before: Memory usage before change (in bytes)
        after: Memory usage after change (in bytes)
        threshold: Optional percentage threshold for "significant" change.
                   If provided, adds supplemental emoji (🎉/🚨/🔸/✅) to chart icons.
                   If None, only shows chart icons (📈/📉/➡️).

    Returns:
        Formatted string with delta and percentage
    """
    delta = after - before
    percentage = 0.0 if before == 0 else (delta / before) * 100

    # Always use chart icons to show direction
    if delta > 0:
        delta_str = f"+{delta:,} bytes"
        trend_icon = "📈"
        # Add supplemental emoji based on threshold if provided
        if threshold is not None:
            significance = "🚨" if abs(percentage) > threshold else "🔸"
            emoji = f"{trend_icon} {significance}"
        else:
            emoji = trend_icon
    elif delta < 0:
        delta_str = f"{delta:,} bytes"
        trend_icon = "📉"
        # Add supplemental emoji based on threshold if provided
        if threshold is not None:
            significance = "🎉" if abs(percentage) > threshold else "✅"
            emoji = f"{trend_icon} {significance}"
        else:
            emoji = trend_icon
    else:
        delta_str = "+0 bytes"
        emoji = "➡️"

    # Format percentage with sign
    if percentage > 0:
        pct_str = f"+{percentage:.2f}%"
    elif percentage < 0:
        pct_str = f"{percentage:.2f}%"
    else:
        pct_str = "0.00%"

    return f"{emoji} {delta_str} ({pct_str})"


def prepare_symbol_changes_data(
    target_symbols: dict | None, pr_symbols: dict | None
) -> dict | None:
    """Prepare symbol changes data for template rendering.

    Args:
        target_symbols: Symbol name to size mapping for target branch
        pr_symbols: Symbol name to size mapping for PR branch

    Returns:
        Dictionary with changed, new, and removed symbols, or None if no changes
    """
    if not target_symbols or not pr_symbols:
        return None

    # Find all symbols that exist in both branches or only in one
    all_symbols = set(target_symbols.keys()) | set(pr_symbols.keys())

    # Track changes
    changed_symbols: list[
        tuple[str, int, int, int]
    ] = []  # (symbol, target_size, pr_size, delta)
    new_symbols: list[tuple[str, int]] = []  # (symbol, size)
    removed_symbols: list[tuple[str, int]] = []  # (symbol, size)

    for symbol in all_symbols:
        target_size = target_symbols.get(symbol, 0)
        pr_size = pr_symbols.get(symbol, 0)

        if target_size == 0 and pr_size > 0:
            # New symbol
            new_symbols.append((symbol, pr_size))
        elif target_size > 0 and pr_size == 0:
            # Removed symbol
            removed_symbols.append((symbol, target_size))
        elif target_size != pr_size:
            # Changed symbol
            delta = pr_size - target_size
            changed_symbols.append((symbol, target_size, pr_size, delta))

    if not changed_symbols and not new_symbols and not removed_symbols:
        return None

    # Sort by size/delta
    changed_symbols.sort(key=lambda x: abs(x[3]), reverse=True)
    new_symbols.sort(key=lambda x: x[1], reverse=True)
    removed_symbols.sort(key=lambda x: x[1], reverse=True)

    return {
        "changed_symbols": changed_symbols,
        "new_symbols": new_symbols,
        "removed_symbols": removed_symbols,
    }


def _truncate_lines(lines: list[str], max_lines: int) -> str:
    """Truncate a list of lines and join into a string.

    Args:
        lines: Lines to truncate
        max_lines: Maximum number of lines to keep

    Returns:
        Joined string with truncation indicator if needed
    """
    if len(lines) > max_lines:
        remaining = len(lines) - max_lines
        lines = lines[:max_lines]
        lines.append(f"... ({remaining} more lines)")
    return "\n".join(lines)


def _count_instructions(asm: str | None) -> int:
    """Count instructions in normalized disassembly text.

    Skips comment lines (source annotations starting with #).
    """
    if not asm:
        return 0
    return sum(1 for line in asm.splitlines() if not line.startswith("# "))


# Pattern to normalize register names for comparison purposes.
# Replaces register operands (a0-a15, r0-r15, sp, lr, pc) with a placeholder
# so that register allocation differences don't cause false diffs.
_REG_NORMALIZE_RE = re.compile(
    r"\ba(?:1[0-5]|[0-9])\b|r(?:1[0-5]|[0-9])\b|\bsp\b|\blr\b|\bpc\b"
)


def _strip_registers(insn: str) -> str:
    """Replace register names with placeholders for comparison."""
    return _REG_NORMALIZE_RE.sub("REG", insn)


def _parse_source_blocks(asm: str) -> list[tuple[str, str]]:
    """Parse normalized asm into source-annotated blocks.

    Each block is (source_annotation, instructions_text) where
    instructions_text contains one or more instruction lines joined by newlines.
    The first block may have an empty annotation if instructions precede
    any source line.

    Returns:
        List of (annotation, instructions) tuples
    """
    blocks: list[tuple[str, str]] = []
    current_annotation = ""
    current_insns: list[str] = []

    for line in asm.splitlines():
        if line.startswith("# "):
            if current_insns:
                blocks.append((current_annotation, "\n".join(current_insns)))
                current_insns = []
            current_annotation = line
        else:
            current_insns.append(line)

    if current_insns:
        blocks.append((current_annotation, "\n".join(current_insns)))

    return blocks


def _source_block_diff(target_asm: str, pr_asm: str) -> list[str] | None:
    """Compute a hybrid source-block / instruction-level diff.

    Uses a two-level approach:
    1. Align source blocks by their annotation header using SequenceMatcher
    2. For blocks with the same header: do instruction-level diff (compact)
    3. For blocks with different headers: show full replacement (new logic)

    This avoids showing entire blocks as replaced when only one instruction
    changed within them, while still clearly showing blocks where the
    source-level logic is completely different.

    Returns:
        List of diff lines (prefixed with +/-/space) or None if no changes.
    """
    target_blocks = _parse_source_blocks(target_asm)
    pr_blocks = _parse_source_blocks(pr_asm)

    # Align blocks by source annotation headers
    target_headers = [h for h, _ in target_blocks]
    pr_headers = [h for h, _ in pr_blocks]

    if target_blocks == pr_blocks:
        return None

    sm = difflib.SequenceMatcher(None, target_headers, pr_headers)
    result: list[str] = []
    has_output = False

    def _add_separator() -> None:
        nonlocal has_output
        if has_output and result and result[-1] != "...":
            result.append("...")
        has_output = True

    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            # Same headers — check if instructions differ within each block
            for ti, pi in zip(range(i1, i2), range(j1, j2)):
                t_header, t_body = target_blocks[ti]
                _, p_body = pr_blocks[pi]
                if t_body == p_body:
                    continue  # Identical block — skip entirely

                t_lines = t_body.splitlines()
                p_lines = p_body.splitlines()

                # Compare with registers stripped to ignore allocation noise.
                # If the only differences are register names, skip the block.
                t_stripped = [_strip_registers(line) for line in t_lines]
                p_stripped = [_strip_registers(line) for line in p_lines]
                if t_stripped == p_stripped:
                    continue  # Only register allocation changed — skip

                # Same source annotation, structural differences — line-level diff
                _add_separator()
                if t_header:
                    result.append(f" {t_header}")
                for dl in difflib.unified_diff(t_lines, p_lines, lineterm="", n=2):
                    if dl.startswith("---") or dl.startswith("+++"):
                        continue
                    if dl.startswith("@@"):
                        if result and result[-1] != "...":
                            result.append("...")
                        continue
                    result.append(dl)
            continue

        # Different headers — full block replacement (logic actually changed)
        _add_separator()

        def _emit_blocks(blocks, start, end, prefix):
            for idx in range(start, end):
                header, body = blocks[idx]
                if header:
                    result.append(f"{prefix}{header}")
                result.extend(f"{prefix}{line}" for line in body.splitlines())

        if tag in ("replace", "delete"):
            _emit_blocks(target_blocks, i1, i2, "-")

        if tag in ("replace", "insert"):
            _emit_blocks(pr_blocks, j1, j2, "+")

    return result or None


def prepare_disassembly_diffs(
    target_disasm: dict | None,
    pr_disasm: dict | None,
    symbol_changes: dict | None,
) -> list[tuple[str, str, str, int, int]] | None:
    """Prepare disassembly diffs for changed symbols.

    Uses source-block-level diffing to show only the blocks where generated
    code actually changed, reducing noise from instruction reordering and
    encoding differences in unchanged sections.

    Args:
        target_disasm: Symbol name to disassembly mapping for target branch
        pr_disasm: Symbol name to disassembly mapping for PR branch
        symbol_changes: Output from prepare_symbol_changes_data()

    Returns:
        List of (symbol_name, change_type, diff_text, target_insns, pr_insns) tuples,
        or None if no diffs.
        change_type is one of: "changed", "new", "removed"
    """
    if not symbol_changes:
        return None
    if not target_disasm and not pr_disasm:
        return None

    target_disasm = target_disasm or {}
    pr_disasm = pr_disasm or {}

    diffs: list[tuple[str, str, str, int, int]] = []
    total_chars = 0

    def _budget_exceeded() -> bool:
        return (
            total_chars >= MAX_DISASM_TOTAL_CHARS
            or len(diffs) >= MAX_DISASM_DIFF_SYMBOLS
        )

    # Changed symbols - show block-level diff
    for symbol, _target_size, _pr_size, _delta in symbol_changes.get(
        "changed_symbols", []
    ):
        if _budget_exceeded():
            break

        target_asm = target_disasm.get(symbol)
        pr_asm = pr_disasm.get(symbol)
        if target_asm is None or pr_asm is None:
            continue
        if target_asm == pr_asm:
            continue

        diff_lines = _source_block_diff(target_asm, pr_asm)
        if not diff_lines:
            continue

        diff_text = _truncate_lines(diff_lines, MAX_DISASM_DIFF_LINES)
        total_chars += len(diff_text)
        diffs.append(
            (
                symbol,
                "changed",
                diff_text,
                _count_instructions(target_asm),
                _count_instructions(pr_asm),
            )
        )

    # New/removed symbols - show full disassembly
    for change_type, symbol_list, disasm_source in [
        ("new", symbol_changes.get("new_symbols", []), pr_disasm),
        ("removed", symbol_changes.get("removed_symbols", []), target_disasm),
    ]:
        for symbol, _size in symbol_list:
            if _budget_exceeded():
                break
            asm = disasm_source.get(symbol)
            if asm is None:
                continue
            insn_count = _count_instructions(asm)
            asm_text = _truncate_lines(asm.splitlines(), MAX_DISASM_NEW_LINES)
            total_chars += len(asm_text)
            if change_type == "new":
                diffs.append((symbol, change_type, asm_text, 0, insn_count))
            else:
                diffs.append((symbol, change_type, asm_text, insn_count, 0))

    return diffs or None


def format_components_str(components: list[str]) -> str:
    """Format a list of components for display.

    Args:
        components: List of component names

    Returns:
        Formatted string with backtick-quoted component names
    """
    if len(components) == 1:
        return f"`{components[0]}`"
    return ", ".join(f"`{c}`" for c in sorted(components))


def prepare_component_breakdown_data(
    target_analysis: dict | None, pr_analysis: dict | None
) -> list[tuple[str, int, int, int]] | None:
    """Prepare component breakdown data for template rendering.

    Args:
        target_analysis: Component memory breakdown for target branch
        pr_analysis: Component memory breakdown for PR branch

    Returns:
        List of tuples (component, target_flash, pr_flash, delta), or None if no changes
    """
    if not target_analysis or not pr_analysis:
        return None

    # Combine all components from both analyses
    all_components = set(target_analysis.keys()) | set(pr_analysis.keys())

    # Filter to components that have changed (ignoring noise)
    changed_components: list[
        tuple[str, int, int, int]
    ] = []  # (comp, target_flash, pr_flash, delta)
    for comp in all_components:
        target_mem = target_analysis.get(comp, {})
        pr_mem = pr_analysis.get(comp, {})

        target_flash = target_mem.get("flash_total", 0)
        pr_flash = pr_mem.get("flash_total", 0)

        # Only include if component has meaningful change (above noise threshold)
        delta = pr_flash - target_flash
        if abs(delta) > COMPONENT_CHANGE_NOISE_THRESHOLD:
            changed_components.append((comp, target_flash, pr_flash, delta))

    if not changed_components:
        return None

    # Sort by absolute delta (largest changes first)
    changed_components.sort(key=lambda x: abs(x[3]), reverse=True)

    return changed_components


def create_comment_body(
    components: list[str],
    platform: str,
    target_ram: int,
    target_flash: int,
    pr_ram: int,
    pr_flash: int,
    target_analysis: dict | None = None,
    pr_analysis: dict | None = None,
    target_symbols: dict | None = None,
    pr_symbols: dict | None = None,
    target_disasm: dict | None = None,
    pr_disasm: dict | None = None,
) -> str:
    """Create the comment body with memory impact analysis using Jinja2 templates.

    Args:
        components: List of component names (merged config)
        platform: Platform name
        target_ram: RAM usage in target branch
        target_flash: Flash usage in target branch
        pr_ram: RAM usage in PR branch
        pr_flash: Flash usage in PR branch
        target_analysis: Optional component breakdown for target branch
        pr_analysis: Optional component breakdown for PR branch
        target_symbols: Optional symbol map for target branch
        pr_symbols: Optional symbol map for PR branch
        target_disasm: Optional disassembly map for target branch
        pr_disasm: Optional disassembly map for PR branch

    Returns:
        Formatted comment body
    """
    # Set up Jinja2 environment
    env = Environment(
        loader=FileSystemLoader(TEMPLATE_DIR),
        trim_blocks=True,
        lstrip_blocks=True,
    )

    # Register custom filters
    env.filters["format_bytes"] = format_bytes
    env.filters["format_change"] = format_change

    # Prepare template context
    context = {
        "comment_marker": COMMENT_MARKER,
        "platform": platform,
        "target_ram": format_bytes(target_ram),
        "pr_ram": format_bytes(pr_ram),
        "target_flash": format_bytes(target_flash),
        "pr_flash": format_bytes(pr_flash),
        "ram_change": format_change(
            target_ram, pr_ram, threshold=OVERALL_CHANGE_THRESHOLD
        ),
        "flash_change": format_change(
            target_flash, pr_flash, threshold=OVERALL_CHANGE_THRESHOLD
        ),
        "component_change_threshold": COMPONENT_CHANGE_THRESHOLD,
    }

    # Format components list
    context["components_str"] = format_components_str(components)
    if len(components) == 1:
        context["config_note"] = "a representative test configuration"
    else:
        context["config_note"] = (
            f"a merged configuration with {len(components)} components"
        )

    # Prepare component breakdown if available
    component_breakdown = ""
    if target_analysis and pr_analysis:
        changed_components = prepare_component_breakdown_data(
            target_analysis, pr_analysis
        )
        if changed_components:
            template = env.get_template("ci_memory_impact_component_breakdown.j2")
            component_breakdown = template.render(
                changed_components=changed_components,
                format_bytes=format_bytes,
                format_change=format_change,
                component_change_threshold=COMPONENT_CHANGE_THRESHOLD,
                max_rows=MAX_COMPONENT_BREAKDOWN_ROWS,
            )

    # Prepare symbol changes if available
    symbol_changes = ""
    disasm_section = ""
    if target_symbols and pr_symbols:
        symbol_data = prepare_symbol_changes_data(target_symbols, pr_symbols)
        if symbol_data:
            template = env.get_template("ci_memory_impact_symbol_changes.j2")
            symbol_changes = template.render(
                **symbol_data,
                format_bytes=format_bytes,
                format_change=format_change,
                max_changed_rows=MAX_CHANGED_SYMBOLS_ROWS,
                max_new_rows=MAX_NEW_SYMBOLS_ROWS,
                max_removed_rows=MAX_REMOVED_SYMBOLS_ROWS,
                symbol_max_length=SYMBOL_DISPLAY_MAX_LENGTH,
                symbol_truncate_length=SYMBOL_DISPLAY_TRUNCATE_LENGTH,
            )

            # Prepare disassembly diffs for changed symbols
            if target_disasm or pr_disasm:
                total_changed = (
                    len(symbol_data.get("changed_symbols", []))
                    + len(symbol_data.get("new_symbols", []))
                    + len(symbol_data.get("removed_symbols", []))
                )
                if total_changed > MAX_DISASM_TOTAL_CHANGED_SYMBOLS:
                    disasm_section = (
                        f"\n> Assembly analysis skipped:"
                        f" {total_changed} symbols changed"
                        f" (threshold: {MAX_DISASM_TOTAL_CHANGED_SYMBOLS}).\n"
                    )
                else:
                    disasm_diffs = prepare_disassembly_diffs(
                        target_disasm, pr_disasm, symbol_data
                    )
                    if disasm_diffs:
                        template = env.get_template("ci_memory_impact_disassembly.j2")
                        disasm_section = template.render(
                            disasm_diffs=disasm_diffs,
                            symbol_max_length=SYMBOL_DISPLAY_MAX_LENGTH,
                            symbol_truncate_length=SYMBOL_DISPLAY_TRUNCATE_LENGTH,
                        )

    if not target_analysis or not pr_analysis:
        print("No ELF files provided, skipping detailed analysis", file=sys.stderr)

    context["component_breakdown"] = component_breakdown
    context["symbol_changes"] = symbol_changes
    context["disasm_section"] = disasm_section

    # Render main template
    template = env.get_template("ci_memory_impact_comment_template.j2")
    return template.render(**context)


def find_existing_comment(pr_number: str) -> str | None:
    """Find existing memory impact comment on the PR.

    Args:
        pr_number: PR number

    Returns:
        Comment numeric ID if found, None otherwise

    Raises:
        subprocess.CalledProcessError: If gh command fails
    """
    print(f"DEBUG: Looking for existing comment on PR #{pr_number}", file=sys.stderr)

    # Use gh api to get comments directly - this returns the numeric id field
    result = run_gh_command(
        [
            "gh",
            "api",
            f"/repos/{{owner}}/{{repo}}/issues/{pr_number}/comments",
            "--jq",
            ".[] | {id, body}",
        ],
        operation="Get PR comments",
    )

    print(
        f"DEBUG: gh api comments output (first 500 chars):\n{result.stdout[:500]}",
        file=sys.stderr,
    )

    # Parse comments and look for our marker
    comment_count = 0
    for line in result.stdout.strip().split("\n"):
        if not line:
            continue

        try:
            comment = json.loads(line)
            comment_count += 1
            comment_id = comment.get("id")
            print(
                f"DEBUG: Checking comment {comment_count}: id={comment_id}",
                file=sys.stderr,
            )

            body = comment.get("body", "")
            if COMMENT_MARKER in body:
                print(
                    f"DEBUG: Found existing comment with id={comment_id}",
                    file=sys.stderr,
                )
                # Return the numeric id
                return str(comment_id)
            print("DEBUG: Comment does not contain marker", file=sys.stderr)
        except json.JSONDecodeError as e:
            print(f"DEBUG: JSON decode error: {e}", file=sys.stderr)
            continue

    print(
        f"DEBUG: No existing comment found (checked {comment_count} comments)",
        file=sys.stderr,
    )
    return None


def update_existing_comment(comment_id: str, comment_body: str) -> None:
    """Update an existing comment.

    Args:
        comment_id: Comment ID to update
        comment_body: New comment body text

    Raises:
        subprocess.CalledProcessError: If gh command fails
    """
    print(f"DEBUG: Updating existing comment {comment_id}", file=sys.stderr)
    print(f"DEBUG: Comment body length: {len(comment_body)} bytes", file=sys.stderr)
    result = run_gh_command(
        [
            "gh",
            "api",
            f"/repos/{{owner}}/{{repo}}/issues/comments/{comment_id}",
            "-X",
            "PATCH",
            "-f",
            f"body={comment_body}",
        ],
        operation="Update PR comment",
    )
    print(f"DEBUG: Update response: {result.stdout}", file=sys.stderr)


def create_new_comment(pr_number: str, comment_body: str) -> None:
    """Create a new PR comment.

    Args:
        pr_number: PR number
        comment_body: Comment body text

    Raises:
        subprocess.CalledProcessError: If gh command fails
    """
    print(f"DEBUG: Posting new comment on PR #{pr_number}", file=sys.stderr)
    print(f"DEBUG: Comment body length: {len(comment_body)} bytes", file=sys.stderr)
    result = run_gh_command(
        ["gh", "pr", "comment", pr_number, "--body", comment_body],
        operation="Create PR comment",
    )
    print(f"DEBUG: Post response: {result.stdout}", file=sys.stderr)


def post_or_update_comment(pr_number: str, comment_body: str) -> None:
    """Post a new comment or update existing one.

    Args:
        pr_number: PR number
        comment_body: Comment body text

    Raises:
        subprocess.CalledProcessError: If gh command fails
    """
    # Look for existing comment
    existing_comment_id = find_existing_comment(pr_number)

    if existing_comment_id and existing_comment_id != "None":
        update_existing_comment(existing_comment_id, comment_body)
    else:
        create_new_comment(pr_number, comment_body)

    print("Comment posted/updated successfully", file=sys.stderr)


def create_target_unavailable_comment(
    pr_data: dict,
) -> str:
    """Create a comment body when target branch data is unavailable.

    This happens when the target branch (dev/beta/release) fails to build.
    This can occur because:
    1. The target branch has a build issue independent of this PR
    2. This PR fixes a build issue on the target branch
    In either case, we only care that the PR branch builds successfully.

    Args:
        pr_data: Dictionary with PR branch analysis results

    Returns:
        Formatted comment body
    """
    components = pr_data.get("components", [])
    platform = pr_data.get("platform", "unknown")
    pr_ram = pr_data.get("ram_bytes", 0)
    pr_flash = pr_data.get("flash_bytes", 0)

    env = Environment(
        loader=FileSystemLoader(TEMPLATE_DIR),
        trim_blocks=True,
        lstrip_blocks=True,
    )
    template = env.get_template("ci_memory_impact_target_unavailable.j2")
    return template.render(
        comment_marker=COMMENT_MARKER,
        components_str=format_components_str(components),
        platform=platform,
        pr_ram=format_bytes(pr_ram),
        pr_flash=format_bytes(pr_flash),
    )


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Post or update PR comment with memory impact analysis"
    )
    parser.add_argument("--pr-number", required=True, help="PR number")
    parser.add_argument(
        "--target-json",
        required=True,
        help="Path to target branch analysis JSON file",
    )
    parser.add_argument(
        "--pr-json",
        required=True,
        help="Path to PR branch analysis JSON file",
    )

    args = parser.parse_args()

    # Load analysis JSON files (all data comes from JSON for security)
    target_data: dict | None = load_analysis_json(args.target_json)
    pr_data: dict | None = load_analysis_json(args.pr_json)

    # PR data is required - if the PR branch can't build, that's a real error
    if not pr_data:
        print("Error: Failed to load PR analysis JSON", file=sys.stderr)
        sys.exit(1)

    # Target data is optional - target branch (dev) may fail to build because:
    # 1. The target branch has a build issue independent of this PR
    # 2. This PR fixes a build issue on the target branch
    if not target_data:
        print(
            "Warning: Target branch analysis unavailable, posting limited comment",
            file=sys.stderr,
        )
        comment_body = create_target_unavailable_comment(pr_data)
        post_or_update_comment(args.pr_number, comment_body)
        return 0

    # Extract detailed analysis if available
    target_analysis: dict | None = None
    pr_analysis: dict | None = None
    target_symbols: dict | None = None
    pr_symbols: dict | None = None
    target_disasm: dict | None = None
    pr_disasm: dict | None = None

    if target_data.get("detailed_analysis"):
        target_analysis = target_data["detailed_analysis"].get("components")
        target_symbols = target_data["detailed_analysis"].get("symbols")
        target_disasm = target_data["detailed_analysis"].get("disassembly")

    if pr_data.get("detailed_analysis"):
        pr_analysis = pr_data["detailed_analysis"].get("components")
        pr_symbols = pr_data["detailed_analysis"].get("symbols")
        pr_disasm = pr_data["detailed_analysis"].get("disassembly")

    # Extract all values from JSON files (prevents shell injection from PR code)
    components = target_data.get("components")
    platform = target_data.get("platform")
    target_ram = target_data.get("ram_bytes")
    target_flash = target_data.get("flash_bytes")
    pr_ram = pr_data.get("ram_bytes")
    pr_flash = pr_data.get("flash_bytes")

    # Validate required fields and types
    missing_fields: list[str] = []
    type_errors: list[str] = []

    if components is None:
        missing_fields.append("components")
    elif not isinstance(components, list):
        type_errors.append(
            f"components must be a list, got {type(components).__name__}"
        )
    else:
        for idx, comp in enumerate(components):
            if not isinstance(comp, str):
                type_errors.append(
                    f"components[{idx}] must be a string, got {type(comp).__name__}"
                )
    if platform is None:
        missing_fields.append("platform")
    elif not isinstance(platform, str):
        type_errors.append(f"platform must be a string, got {type(platform).__name__}")

    if target_ram is None:
        missing_fields.append("target.ram_bytes")
    elif not isinstance(target_ram, int):
        type_errors.append(
            f"target.ram_bytes must be an integer, got {type(target_ram).__name__}"
        )

    if target_flash is None:
        missing_fields.append("target.flash_bytes")
    elif not isinstance(target_flash, int):
        type_errors.append(
            f"target.flash_bytes must be an integer, got {type(target_flash).__name__}"
        )

    if pr_ram is None:
        missing_fields.append("pr.ram_bytes")
    elif not isinstance(pr_ram, int):
        type_errors.append(
            f"pr.ram_bytes must be an integer, got {type(pr_ram).__name__}"
        )

    if pr_flash is None:
        missing_fields.append("pr.flash_bytes")
    elif not isinstance(pr_flash, int):
        type_errors.append(
            f"pr.flash_bytes must be an integer, got {type(pr_flash).__name__}"
        )

    if missing_fields or type_errors:
        if missing_fields:
            print(
                f"Error: JSON files missing required fields: {', '.join(missing_fields)}",
                file=sys.stderr,
            )
        if type_errors:
            print(
                f"Error: Type validation failed: {'; '.join(type_errors)}",
                file=sys.stderr,
            )
        print(f"Target JSON keys: {list(target_data.keys())}", file=sys.stderr)
        print(f"PR JSON keys: {list(pr_data.keys())}", file=sys.stderr)
        sys.exit(1)

    # Create comment body
    # Note: Memory totals (RAM/Flash) are summed across all builds if multiple were run.
    comment_body = create_comment_body(
        components=components,
        platform=platform,
        target_ram=target_ram,
        target_flash=target_flash,
        pr_ram=pr_ram,
        pr_flash=pr_flash,
        target_analysis=target_analysis,
        pr_analysis=pr_analysis,
        target_symbols=target_symbols,
        pr_symbols=pr_symbols,
        target_disasm=target_disasm,
        pr_disasm=pr_disasm,
    )

    # Post or update comment
    post_or_update_comment(args.pr_number, comment_body)

    return 0


if __name__ == "__main__":
    sys.exit(main())
