#!/usr/bin/env python3
"""Post or update a PR comment with memory impact analysis results.

This script creates or updates a GitHub PR comment with memory usage changes.
It uses the GitHub CLI (gh) to manage comments and maintains a single comment
that gets updated on subsequent runs.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys

# Comment marker to identify our memory impact comments
COMMENT_MARKER = "<!-- esphome-memory-impact-analysis -->"


def format_bytes(bytes_value: int) -> str:
    """Format bytes value with appropriate unit.

    Args:
        bytes_value: Number of bytes

    Returns:
        Formatted string (e.g., "1.5 KB", "256 bytes")
    """
    if bytes_value < 1024:
        return f"{bytes_value} bytes"
    if bytes_value < 1024 * 1024:
        return f"{bytes_value / 1024:.2f} KB"
    return f"{bytes_value / (1024 * 1024):.2f} MB"


def format_change(before: int, after: int) -> str:
    """Format memory change with delta and percentage.

    Args:
        before: Memory usage before change
        after: Memory usage after change

    Returns:
        Formatted string with delta and percentage
    """
    delta = after - before
    percentage = 0.0 if before == 0 else (delta / before) * 100

    # Format delta with sign
    delta_str = f"+{format_bytes(delta)}" if delta >= 0 else format_bytes(delta)

    # Format percentage with sign
    if percentage > 0:
        pct_str = f"+{percentage:.2f}%"
    elif percentage < 0:
        pct_str = f"{percentage:.2f}%"
    else:
        pct_str = "0.00%"

    # Add emoji indicator
    if delta > 0:
        emoji = "📈"
    elif delta < 0:
        emoji = "📉"
    else:
        emoji = "➡️"

    return f"{emoji} {delta_str} ({pct_str})"


def create_comment_body(
    component: str,
    platform: str,
    target_ram: int,
    target_flash: int,
    pr_ram: int,
    pr_flash: int,
) -> str:
    """Create the comment body with memory impact analysis.

    Args:
        component: Component name
        platform: Platform name
        target_ram: RAM usage in target branch
        target_flash: Flash usage in target branch
        pr_ram: RAM usage in PR branch
        pr_flash: Flash usage in PR branch

    Returns:
        Formatted comment body
    """
    ram_change = format_change(target_ram, pr_ram)
    flash_change = format_change(target_flash, pr_flash)

    return f"""{COMMENT_MARKER}
## Memory Impact Analysis

**Component:** `{component}`
**Platform:** `{platform}`

| Metric | Target Branch | This PR | Change |
|--------|--------------|---------|--------|
| **RAM** | {format_bytes(target_ram)} | {format_bytes(pr_ram)} | {ram_change} |
| **Flash** | {format_bytes(target_flash)} | {format_bytes(pr_flash)} | {flash_change} |

---
*This analysis runs automatically when a single component changes. Memory usage is measured from a representative test configuration.*
"""


def find_existing_comment(pr_number: str) -> str | None:
    """Find existing memory impact comment on the PR.

    Args:
        pr_number: PR number

    Returns:
        Comment ID if found, None otherwise
    """
    try:
        # List all comments on the PR
        result = subprocess.run(
            [
                "gh",
                "pr",
                "view",
                pr_number,
                "--json",
                "comments",
                "--jq",
                ".comments[]",
            ],
            capture_output=True,
            text=True,
            check=True,
        )

        # Parse comments and look for our marker
        for line in result.stdout.strip().split("\n"):
            if not line:
                continue

            try:
                comment = json.loads(line)
                if COMMENT_MARKER in comment.get("body", ""):
                    return str(comment["id"])
            except json.JSONDecodeError:
                continue

        return None

    except subprocess.CalledProcessError as e:
        print(f"Error finding existing comment: {e}", file=sys.stderr)
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
        if existing_comment_id:
            # Update existing comment
            print(f"Updating existing comment {existing_comment_id}", file=sys.stderr)
            subprocess.run(
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
            )
        else:
            # Post new comment
            print("Posting new comment", file=sys.stderr)
            subprocess.run(
                ["gh", "pr", "comment", pr_number, "--body", comment_body],
                check=True,
                capture_output=True,
            )

        print("Comment posted/updated successfully", file=sys.stderr)
        return True

    except subprocess.CalledProcessError as e:
        print(f"Error posting/updating comment: {e}", file=sys.stderr)
        if e.stderr:
            print(f"stderr: {e.stderr.decode()}", file=sys.stderr)
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

    args = parser.parse_args()

    # Create comment body
    comment_body = create_comment_body(
        component=args.component,
        platform=args.platform,
        target_ram=args.target_ram,
        target_flash=args.target_flash,
        pr_ram=args.pr_ram,
        pr_flash=args.pr_flash,
    )

    # Post or update comment
    success = post_or_update_comment(args.pr_number, comment_body)

    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
