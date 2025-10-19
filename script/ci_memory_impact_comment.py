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

from jinja2 import Environment, FileSystemLoader

# Add esphome to path for analyze_memory import
sys.path.insert(0, str(Path(__file__).parent.parent))

# pylint: disable=wrong-import-position

# Comment marker to identify our memory impact comments
COMMENT_MARKER = "<!-- esphome-memory-impact-analysis -->"

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
    target_cache_hit: bool = False,
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
        target_cache_hit: Whether target branch analysis was loaded from cache

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
        "target_cache_hit": target_cache_hit,
        "component_change_threshold": COMPONENT_CHANGE_THRESHOLD,
    }

    # Format components list
    if len(components) == 1:
        context["components_str"] = f"`{components[0]}`"
        context["config_note"] = "a representative test configuration"
    else:
        context["components_str"] = ", ".join(f"`{c}`" for c in sorted(components))
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

    if not target_analysis or not pr_analysis:
        print("No ELF files provided, skipping detailed analysis", file=sys.stderr)

    context["component_breakdown"] = component_breakdown
    context["symbol_changes"] = symbol_changes

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


def update_existing_comment(comment_id: str, comment_body: str) -> None:
    """Update an existing comment.

    Args:
        comment_id: Comment ID to update
        comment_body: New comment body text

    Raises:
        subprocess.CalledProcessError: If gh command fails
    """
    print(f"DEBUG: Updating existing comment {comment_id}", file=sys.stderr)
    result = subprocess.run(
        [
            "gh",
            "api",
            f"/repos/{{owner}}/{{repo}}/issues/comments/{comment_id}",
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


def create_new_comment(pr_number: str, comment_body: str) -> None:
    """Create a new PR comment.

    Args:
        pr_number: PR number
        comment_body: Comment body text

    Raises:
        subprocess.CalledProcessError: If gh command fails
    """
    print(f"DEBUG: Posting new comment on PR #{pr_number}", file=sys.stderr)
    result = subprocess.run(
        ["gh", "pr", "comment", pr_number, "--body", comment_body],
        check=True,
        capture_output=True,
        text=True,
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


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Post or update PR comment with memory impact analysis"
    )
    parser.add_argument("--pr-number", required=True, help="PR number")
    parser.add_argument(
        "--components",
        required=True,
        help='JSON array of component names (e.g., \'["api", "wifi"]\')',
    )
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
    parser.add_argument(
        "--target-json",
        help="Optional path to target branch analysis JSON (for detailed analysis)",
    )
    parser.add_argument(
        "--pr-json",
        help="Optional path to PR branch analysis JSON (for detailed analysis)",
    )
    parser.add_argument(
        "--target-cache-hit",
        action="store_true",
        help="Indicates that target branch analysis was loaded from cache",
    )

    args = parser.parse_args()

    # Parse components from JSON
    try:
        components = json.loads(args.components)
        if not isinstance(components, list):
            print("Error: --components must be a JSON array", file=sys.stderr)
            sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error parsing --components JSON: {e}", file=sys.stderr)
        sys.exit(1)

    # Load analysis JSON files
    target_analysis = None
    pr_analysis = None
    target_symbols = None
    pr_symbols = None

    if args.target_json:
        target_data = load_analysis_json(args.target_json)
        if target_data and target_data.get("detailed_analysis"):
            target_analysis = target_data["detailed_analysis"].get("components")
            target_symbols = target_data["detailed_analysis"].get("symbols")

    if args.pr_json:
        pr_data = load_analysis_json(args.pr_json)
        if pr_data and pr_data.get("detailed_analysis"):
            pr_analysis = pr_data["detailed_analysis"].get("components")
            pr_symbols = pr_data["detailed_analysis"].get("symbols")

    # Create comment body
    # Note: Memory totals (RAM/Flash) are summed across all builds if multiple were run.
    comment_body = create_comment_body(
        components=components,
        platform=args.platform,
        target_ram=args.target_ram,
        target_flash=args.target_flash,
        pr_ram=args.pr_ram,
        pr_flash=args.pr_flash,
        target_analysis=target_analysis,
        pr_analysis=pr_analysis,
        target_symbols=target_symbols,
        pr_symbols=pr_symbols,
        target_cache_hit=args.target_cache_hit,
    )

    # Post or update comment
    post_or_update_comment(args.pr_number, comment_body)

    return 0


if __name__ == "__main__":
    sys.exit(main())
