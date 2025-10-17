#!/usr/bin/env python3
"""Post or update a PR comment with memory impact analysis results.

This script creates or updates a GitHub PR comment with memory usage changes.
It uses the GitHub CLI (gh) to manage comments and maintains a single comment
that gets updated on subsequent runs.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys

# Add esphome to path for analyze_memory import
sys.path.insert(0, str(Path(__file__).parent.parent))

# pylint: disable=wrong-import-position
from esphome.analyze_memory import MemoryAnalyzer

# Comment marker to identify our memory impact comments
COMMENT_MARKER = "<!-- esphome-memory-impact-analysis -->"


def format_bytes(bytes_value: int) -> str:
    """Format bytes value with comma separators.

    Args:
        bytes_value: Number of bytes

    Returns:
        Formatted string with comma separators (e.g., "1,234 bytes")
    """
    return f"{bytes_value:,} bytes"


def format_change(before: int, after: int) -> str:
    """Format memory change with delta and percentage.

    Args:
        before: Memory usage before change (in bytes)
        after: Memory usage after change (in bytes)

    Returns:
        Formatted string with delta and percentage
    """
    delta = after - before
    percentage = 0.0 if before == 0 else (delta / before) * 100

    # Format delta with sign and always show in bytes for precision
    if delta > 0:
        delta_str = f"+{delta:,} bytes"
        emoji = "📈"
    elif delta < 0:
        delta_str = f"{delta:,} bytes"
        emoji = "📉"
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


def run_detailed_analysis(
    elf_path: str, objdump_path: str | None = None, readelf_path: str | None = None
) -> dict | None:
    """Run detailed memory analysis on an ELF file.

    Args:
        elf_path: Path to ELF file
        objdump_path: Optional path to objdump tool
        readelf_path: Optional path to readelf tool

    Returns:
        Dictionary with component memory breakdown or None if analysis fails
    """
    try:
        analyzer = MemoryAnalyzer(elf_path, objdump_path, readelf_path)
        components = analyzer.analyze()

        # Convert ComponentMemory objects to dictionaries
        result = {}
        for name, mem in components.items():
            result[name] = {
                "text": mem.text_size,
                "rodata": mem.rodata_size,
                "data": mem.data_size,
                "bss": mem.bss_size,
                "flash_total": mem.flash_total,
                "ram_total": mem.ram_total,
                "symbol_count": mem.symbol_count,
            }
        return result
    except Exception as e:
        print(f"Warning: Failed to run detailed analysis: {e}", file=sys.stderr)
        return None


def create_detailed_breakdown_table(
    target_analysis: dict | None, pr_analysis: dict | None
) -> str:
    """Create a markdown table showing detailed memory breakdown by component.

    Args:
        target_analysis: Component memory breakdown for target branch
        pr_analysis: Component memory breakdown for PR branch

    Returns:
        Formatted markdown table
    """
    if not target_analysis or not pr_analysis:
        return ""

    # Combine all components from both analyses
    all_components = set(target_analysis.keys()) | set(pr_analysis.keys())

    # Filter to components that have changed or are significant
    changed_components = []
    for comp in all_components:
        target_mem = target_analysis.get(comp, {})
        pr_mem = pr_analysis.get(comp, {})

        target_flash = target_mem.get("flash_total", 0)
        pr_flash = pr_mem.get("flash_total", 0)

        # Include if component has changed or is significant (> 1KB)
        if target_flash != pr_flash or target_flash > 1024 or pr_flash > 1024:
            delta = pr_flash - target_flash
            changed_components.append((comp, target_flash, pr_flash, delta))

    if not changed_components:
        return ""

    # Sort by absolute delta (largest changes first)
    changed_components.sort(key=lambda x: abs(x[3]), reverse=True)

    # Build table - limit to top 20 changes
    lines = [
        "",
        "<details>",
        "<summary>📊 Detailed Memory Breakdown (click to expand)</summary>",
        "",
        "| Component | Target Flash | PR Flash | Change |",
        "|-----------|--------------|----------|--------|",
    ]

    for comp, target_flash, pr_flash, delta in changed_components[:20]:
        target_str = format_bytes(target_flash)
        pr_str = format_bytes(pr_flash)
        change_str = format_change(target_flash, pr_flash)
        lines.append(f"| `{comp}` | {target_str} | {pr_str} | {change_str} |")

    if len(changed_components) > 20:
        lines.append(
            f"| ... | ... | ... | *({len(changed_components) - 20} more components not shown)* |"
        )

    lines.extend(["", "</details>", ""])

    return "\n".join(lines)


def create_comment_body(
    component: str,
    platform: str,
    target_ram: int,
    target_flash: int,
    pr_ram: int,
    pr_flash: int,
    target_elf: str | None = None,
    pr_elf: str | None = None,
    objdump_path: str | None = None,
    readelf_path: str | None = None,
) -> str:
    """Create the comment body with memory impact analysis.

    Args:
        component: Component name
        platform: Platform name
        target_ram: RAM usage in target branch
        target_flash: Flash usage in target branch
        pr_ram: RAM usage in PR branch
        pr_flash: Flash usage in PR branch
        target_elf: Optional path to target branch ELF file
        pr_elf: Optional path to PR branch ELF file
        objdump_path: Optional path to objdump tool
        readelf_path: Optional path to readelf tool

    Returns:
        Formatted comment body
    """
    ram_change = format_change(target_ram, pr_ram)
    flash_change = format_change(target_flash, pr_flash)

    # Run detailed analysis if ELF files are provided
    target_analysis = None
    pr_analysis = None
    detailed_breakdown = ""

    if target_elf and pr_elf:
        print(
            f"Running detailed analysis on {target_elf} and {pr_elf}", file=sys.stderr
        )
        target_analysis = run_detailed_analysis(target_elf, objdump_path, readelf_path)
        pr_analysis = run_detailed_analysis(pr_elf, objdump_path, readelf_path)

        if target_analysis and pr_analysis:
            detailed_breakdown = create_detailed_breakdown_table(
                target_analysis, pr_analysis
            )
    else:
        print("No ELF files provided, skipping detailed analysis", file=sys.stderr)

    return f"""{COMMENT_MARKER}
## Memory Impact Analysis

**Component:** `{component}`
**Platform:** `{platform}`

| Metric | Target Branch | This PR | Change |
|--------|--------------|---------|--------|
| **RAM** | {format_bytes(target_ram)} | {format_bytes(pr_ram)} | {ram_change} |
| **Flash** | {format_bytes(target_flash)} | {format_bytes(pr_flash)} | {flash_change} |
{detailed_breakdown}
---
*This analysis runs automatically when a single component changes. Memory usage is measured from a representative test configuration.*
"""


def find_existing_comment(pr_number: str) -> str | None:
    """Find existing memory impact comment on the PR.

    Args:
        pr_number: PR number

    Returns:
        Comment numeric ID if found, None otherwise
    """
    try:
        print(
            f"DEBUG: Looking for existing comment on PR #{pr_number}", file=sys.stderr
        )

        # Use gh api to get comments directly - this returns the numeric id field
        result = subprocess.run(
            [
                "gh",
                "api",
                f"/repos/{{owner}}/{{repo}}/issues/{pr_number}/comments",
                "--jq",
                ".[] | {id, body}",
            ],
            capture_output=True,
            text=True,
            check=True,
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

    except subprocess.CalledProcessError as e:
        print(f"Error finding existing comment: {e}", file=sys.stderr)
        if e.stderr:
            print(f"stderr: {e.stderr.decode()}", file=sys.stderr)
        return None


def post_or_update_comment(pr_number: str, comment_body: str) -> bool:
    """Post a new comment or update existing one.

    Args:
        pr_number: PR number
        comment_body: Comment body text

    Returns:
        True if successful, False otherwise
    """
    # Look for existing comment
    existing_comment_id = find_existing_comment(pr_number)

    try:
        if existing_comment_id and existing_comment_id != "None":
            # Update existing comment
            print(
                f"DEBUG: Updating existing comment {existing_comment_id}",
                file=sys.stderr,
            )
            result = subprocess.run(
                [
                    "gh",
                    "api",
                    f"/repos/{{owner}}/{{repo}}/issues/comments/{existing_comment_id}",
                    "-X",
                    "PATCH",
                    "-f",
                    f"body={comment_body}",
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            print(f"DEBUG: Update response: {result.stdout}", file=sys.stderr)
        else:
            # Post new comment
            print(
                f"DEBUG: Posting new comment (existing_comment_id={existing_comment_id})",
                file=sys.stderr,
            )
            result = subprocess.run(
                ["gh", "pr", "comment", pr_number, "--body", comment_body],
                check=True,
                capture_output=True,
                text=True,
            )
            print(f"DEBUG: Post response: {result.stdout}", file=sys.stderr)

        print("Comment posted/updated successfully", file=sys.stderr)
        return True

    except subprocess.CalledProcessError as e:
        print(f"Error posting/updating comment: {e}", file=sys.stderr)
        if e.stderr:
            print(
                f"stderr: {e.stderr.decode() if isinstance(e.stderr, bytes) else e.stderr}",
                file=sys.stderr,
            )
        if e.stdout:
            print(
                f"stdout: {e.stdout.decode() if isinstance(e.stdout, bytes) else e.stdout}",
                file=sys.stderr,
            )
        return False


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Post or update PR comment with memory impact analysis"
    )
    parser.add_argument("--pr-number", required=True, help="PR number")
    parser.add_argument("--component", required=True, help="Component name")
    parser.add_argument("--platform", required=True, help="Platform name")
    parser.add_argument(
        "--target-ram", type=int, required=True, help="Target branch RAM usage"
    )
    parser.add_argument(
        "--target-flash", type=int, required=True, help="Target branch flash usage"
    )
    parser.add_argument("--pr-ram", type=int, required=True, help="PR branch RAM usage")
    parser.add_argument(
        "--pr-flash", type=int, required=True, help="PR branch flash usage"
    )
    parser.add_argument("--target-elf", help="Optional path to target branch ELF file")
    parser.add_argument("--pr-elf", help="Optional path to PR branch ELF file")
    parser.add_argument(
        "--objdump-path", help="Optional path to objdump tool for detailed analysis"
    )
    parser.add_argument(
        "--readelf-path", help="Optional path to readelf tool for detailed analysis"
    )

    args = parser.parse_args()

    # Create comment body
    comment_body = create_comment_body(
        component=args.component,
        platform=args.platform,
        target_ram=args.target_ram,
        target_flash=args.target_flash,
        pr_ram=args.pr_ram,
        pr_flash=args.pr_flash,
        target_elf=args.target_elf,
        pr_elf=args.pr_elf,
        objdump_path=args.objdump_path,
        readelf_path=args.readelf_path,
    )

    # Post or update comment
    success = post_or_update_comment(args.pr_number, comment_body)

    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
