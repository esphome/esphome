#!/usr/bin/env python3
"""
ESPHome Release Notes Generator
=================================

This script automates the generation of release notes for ESPHome by:
  1. Discovering PRs merged between releases using GitHub CLI
  2. Caching PR metadata locally
  3. Generating AI prompts for Claude Code CLI
  4. Assembling the final changelog from AI responses and PR data

Prerequisites:
--------------
- Python 3.8+ (recommended: 3.11+)
- GitHub CLI (`gh`) installed and authenticated:
    - Install: https://cli.github.com/
    - Authenticate: `gh auth login`
- Internet access for fetching PR data

Required Dependencies:
---------------------
- `jinja2` (for templating)
    Install via pip: `pip install jinja2`
    Or via uv: `uv pip install jinja2`

Usage:
------
Basic workflow:
  1. Fetch PRs and generate AI prompts:
       python script/generate_release_notes.py 2025.11.0
  2. Force re-fetch PRs (if needed):
       python script/generate_release_notes.py 2025.11.0 --update
  3. Assemble release notes from AI responses:
       python script/generate_release_notes.py 2025.11.0 --assemble

Detailed Workflow:
------------------
Step 1: Generate Prompts
  $ python script/generate_release_notes.py 2025.11.0
  This discovers PRs between the previous release and the current version,
  caches PR metadata, and generates AI prompts in script/cache/2025.11.0/prompts/

Step 2: Process Prompts with Claude Code CLI
  Start Claude Code CLI and read the prompts:
  $ claude
  > Please read script/cache/2025.11.0/prompts/overview_and_highlights.txt and follow the instructions
  > Please read script/cache/2025.11.0/prompts/breaking_changes.txt and follow the instructions

  Claude will write AI responses to script/cache/2025.11.0/ai_responses/

Step 3: Review AI Responses (CRITICAL!)
  Carefully review and edit the AI-generated content in script/cache/2025.11.0/ai_responses/
  Check for:
  - Hallucinations or inaccurate technical claims
  - Incorrect compatibility statements
  - Mischaracterized features

Step 4: Assemble Changelog
  $ python script/generate_release_notes.py 2025.11.0 --assemble
  This combines AI responses with auto-generated PR lists into content/changelog/2025.11.0.md

Troubleshooting Common Issues:
-----------------------------
- "gh: command not found": Install GitHub CLI and ensure it's in your PATH.
- "gh authentication failed": Run `gh auth login` and verify access to the repository.
- "ModuleNotFoundError: No module named 'jinja2'": Install with `pip install jinja2` or `uv pip install jinja2`.
- "No PRs found for version": Ensure the version tag exists and you have network access.
- "Permission denied" or file errors: Check directory permissions and paths.

For further help, see the ESPHome documentation or contact maintainers.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime
import json
from pathlib import Path
import re
import subprocess
import sys

from jinja2 import Environment, FileSystemLoader, select_autoescape

# Label constants
LABEL_BREAKING_CHANGE = "breaking-change"
LABEL_NEW_FEATURE = "new-feature"
LABEL_NEW_COMPONENT = "new-component"


@dataclass
class Version:
    """ESPHome version representation"""

    year: int
    month: int
    patch: int
    beta: int = 0

    def __str__(self):
        base = f"{self.year}.{self.month}.{self.patch}"
        if self.beta > 0:
            base += f"b{self.beta}"
        return base

    @property
    def tag(self):
        """Git tag name"""
        return str(self)

    @classmethod
    def parse(cls, value: str) -> Version:
        """Parse version string like '2025.11.0' or '2025.11.0b1'"""
        match = re.match(r"(\d{4})\.(\d+)\.(\d+)(b(\d+))?", value)
        if not match:
            raise ValueError(
                f"Invalid version format: {value}. Expected format: YYYY.MM.PATCH or YYYY.MM.PATCHbN"
            )
        year = int(match[1])
        month = int(match[2])
        patch = int(match[3])
        beta = int(match[5]) if match[5] else 0
        return cls(year=year, month=month, patch=patch, beta=beta)

    def previous_version_base(self) -> Version:
        """Get the base version of previous month (always .0 patch)"""
        if self.month == 1:
            # January -> previous December
            return Version(year=self.year - 1, month=12, patch=0)
        return Version(year=self.year, month=self.month - 1, patch=0)

    def find_latest_patch(self, all_tags: set[str]) -> Version:
        """Find the latest patch release for this major.minor version"""
        base = f"{self.year}.{self.month}."
        patches = []
        for tag in all_tags:
            if tag.startswith(base) and "b" not in tag:
                try:
                    patches.append(int(tag.replace(base, "").split("b")[0]))
                except ValueError:
                    continue  # Skip malformed tags
        if not patches:
            return self
        max_patch = max(patches)
        return Version(year=self.year, month=self.month, patch=max_patch)

    def find_latest_beta(self, all_tags: set[str]) -> tuple[str, bool]:
        """Find the latest beta tag for this major.minor.0 version

        Returns:
            tuple of (beta_tag, exists) where beta_tag is like "2025.11.0b3"
        """
        base = f"{self.year}.{self.month}.0b"
        betas = []
        for tag in all_tags:
            if tag.startswith(base):
                try:
                    betas.append(int(tag.replace(base, "")))
                except ValueError:
                    continue  # Skip malformed tags
        if not betas:
            return (f"{base}1", False)
        max_beta = max(betas)
        return (f"{base}{max_beta}", True)


@dataclass
class PullRequest:
    """Pull request metadata"""

    number: int
    title: str
    body: str
    author: str
    labels: list[str]
    url: str
    state: str
    merged_at: str | None = None

    @classmethod
    def from_json(cls, data: dict) -> PullRequest:
        """Create PR from GitHub API JSON response"""
        return cls(
            number=data["number"],
            title=data["title"],
            body=data.get("body", ""),
            author=data.get("author", {}).get("login", "unknown")
            if data.get("author")
            else "unknown",
            labels=[label["name"] for label in data.get("labels", [])],
            url=data["url"],
            state=data["state"],
            merged_at=data.get("mergedAt"),
        )

    def to_json(self) -> dict:
        """Convert to JSON-serializable dict"""
        return {
            "number": self.number,
            "title": self.title,
            "body": self.body,
            "author": self.author,
            "labels": self.labels,
            "url": self.url,
            "state": self.state,
            "merged_at": self.merged_at,
        }


class ReleaseNotesGenerator:
    """Main release notes generator"""

    def __init__(
        self, version: Version, force_update: bool = False, dry_run: bool = False
    ):
        self.version = version
        self.force_update = force_update
        self.dry_run = dry_run
        # Shared cache for all PRs (persistent across all versions)
        self.prs_cache_dir = Path("script/cache/prs")
        # Version-specific directories
        self.version_dir = Path("script/cache") / str(version)
        self.prompts_dir = self.version_dir / "prompts"
        self.responses_dir = self.version_dir / "ai_responses"
        self._all_tags: set[str] | None = None

        # Set up Jinja2 environment for templates
        template_dir = Path("script/prompt_templates")
        self.jinja_env = Environment(
            loader=FileSystemLoader(template_dir),
            autoescape=select_autoescape(),
            trim_blocks=True,
            lstrip_blocks=True,
        )

    @staticmethod
    def _print_gh_install_instructions() -> None:
        """Print GitHub CLI installation instructions"""
        print("\nInstallation instructions:")
        print("  macOS:   brew install gh")
        print(
            "  Linux:   See https://github.com/cli/cli/blob/trunk/docs/install_linux.md"
        )
        print("  Windows: See https://github.com/cli/cli#installation")

    def check_github_cli(self) -> None:
        """Check if GitHub CLI is installed and authenticated"""
        try:
            result = subprocess.run(
                ["gh", "--version"],
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode != 0:
                print("Error: GitHub CLI (gh) is not installed or not in PATH")
                self._print_gh_install_instructions()
                sys.exit(1)
        except FileNotFoundError:
            print("Error: GitHub CLI (gh) is not installed")
            self._print_gh_install_instructions()
            sys.exit(1)

        # Check authentication
        try:
            result = subprocess.run(
                ["gh", "auth", "status"],
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode != 0:
                print("Error: GitHub CLI is not authenticated")
                print("\nPlease run: gh auth login")
                sys.exit(1)
        except (FileNotFoundError, OSError) as e:
            print(f"Error checking GitHub CLI authentication: {e}")
            print("\nPlease run: gh auth login")
            sys.exit(1)

    def ensure_dirs(self) -> None:
        """Create cache directories if they don't exist"""
        self.prs_cache_dir.mkdir(parents=True, exist_ok=True)
        self.prompts_dir.mkdir(parents=True, exist_ok=True)
        self.responses_dir.mkdir(parents=True, exist_ok=True)

    def run_gh(self, *args) -> dict:
        """Run gh CLI command and return JSON output"""
        cmd = ["gh"] + list(args)
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=True,
            )
            return json.loads(result.stdout) if result.stdout else {}
        except subprocess.CalledProcessError as e:
            print(f"Error running gh command: {' '.join(cmd)}")
            print(f"stderr: {e.stderr}")
            raise

    def _fetch_all_tags(self) -> set[str]:
        """Fetch all tags from esphome/esphome repo (cached)"""
        if self._all_tags is not None:
            return self._all_tags

        print("Fetching all tags from esphome/esphome...")
        try:
            result = subprocess.run(
                [
                    "gh",
                    "api",
                    "repos/esphome/esphome/tags",
                    "--paginate",
                    "--jq",
                    ".[].name",
                ],
                capture_output=True,
                text=True,
                check=True,
            )
            tags = [t for t in result.stdout.strip().split("\n") if t]
            self._all_tags = set(tags)
            print(f"Found {len(self._all_tags)} tags")
            return self._all_tags
        except subprocess.CalledProcessError as e:
            print(f"Error fetching tags: {e.stderr}", file=sys.stderr)
            print("Failed to fetch tags. Exiting.", file=sys.stderr)
            sys.exit(1)

    def tag_exists(self, tag: str) -> bool:
        """Check if a git tag exists in esphome/esphome repo"""
        all_tags = self._fetch_all_tags()
        return tag in all_tags

    def get_pr_numbers_from_commits(self, base_ref: str, head_ref: str) -> list[int]:
        """Extract PR numbers from commits between two refs"""
        print(f"Comparing {base_ref}...{head_ref}")

        # Use --paginate with --jq to get all commit messages across all pages
        # This automatically handles pagination and extracts just what we need
        result = subprocess.run(
            [
                "gh",
                "api",
                f"repos/esphome/esphome/compare/{base_ref}...{head_ref}",
                "--paginate",
                "--jq",
                ".commits[].commit.message",
            ],
            capture_output=True,
            text=True,
            check=True,
        )

        # Each line is a commit message
        commit_messages = [line for line in result.stdout.strip().split("\n") if line]

        print(f"Found {len(commit_messages)} commits")

        pr_numbers = set()
        for message in commit_messages:
            # Extract PR numbers from patterns like (#12345)
            matches = re.findall(r"\(#(\d+)\)", message)
            pr_numbers.update(int(m) for m in matches)

        return sorted(pr_numbers)

    def _get_patch_release_prs(self, base_version: Version) -> list[int]:
        """Get all PRs that were included in patch releases (e.g., 2025.10.1, 2025.10.2)"""
        patch_prs = set()
        patch_num = 1
        max_patches = 100  # Safety limit to prevent infinite loops

        print(
            f"Checking for patch releases of {base_version.year}.{base_version.month}.x..."
        )

        while patch_num <= max_patches:
            patch_tag = f"{base_version.year}.{base_version.month}.{patch_num}"

            if not self.tag_exists(patch_tag):
                break

            print(f"  Found patch release: {patch_tag}")

            # Get PRs between base and this patch
            base_tag = f"{base_version.year}.{base_version.month}.{patch_num - 1}"
            prs = self.get_pr_numbers_from_commits(base_tag, patch_tag)
            patch_prs.update(prs)

            patch_num += 1

        if patch_num > max_patches:
            print(
                f"Warning: Reached maximum patch limit ({max_patches}). Some patches may have been skipped."
            )

        return sorted(patch_prs)

    def discover_prs(self) -> list[int]:
        """Discover PRs for this release"""
        current_tag = self.version.tag

        # Find the latest patch release of the previous month
        previous_base = self.version.previous_version_base()
        all_tags = self._fetch_all_tags()
        previous_version = previous_base.find_latest_patch(all_tags)
        previous_tag = previous_version.tag

        print(f"\n=== Discovering PRs for {current_tag} ===\n")
        print(f"Previous version: {previous_tag}")

        # Find the latest beta tag (e.g., 2025.11.0b1, b2, b3, etc.)
        beta_tag, beta_tag_exists = self.version.find_latest_beta(all_tags)

        # Check if previous version tag exists
        if not self.tag_exists(previous_tag):
            print(f"Error: Previous version tag '{previous_tag}' does not exist")
            print("Cannot determine which PRs are new")
            sys.exit(1)

        if beta_tag_exists:
            # Beta branch exists - use everything from previous release to beta
            print(f"Beta tag '{beta_tag}' exists")
            print(f"Comparing tags: {previous_tag}...{beta_tag}")
            pr_numbers = self.get_pr_numbers_from_commits(previous_tag, beta_tag)
        else:
            # Beta doesn't exist yet - use dev branch but exclude patch releases
            print(f"Beta tag '{beta_tag}' does not exist yet")
            print("Using dev branch and excluding patch releases")

            # Get all PRs from previous version to dev
            all_prs = self.get_pr_numbers_from_commits(previous_tag, "dev")

            # Find and exclude PRs from patch releases
            patch_prs = self._get_patch_release_prs(previous_version)
            pr_numbers = sorted(set(all_prs) - set(patch_prs))

            if patch_prs:
                print(f"Excluded {len(patch_prs)} PRs from patch releases")

        return pr_numbers

    def fetch_pr(self, pr_number: int) -> PullRequest:
        """Fetch PR metadata from GitHub"""
        print(f"Fetching PR #{pr_number}...", end=" ")

        data = self.run_gh(
            "pr",
            "view",
            str(pr_number),
            "--repo",
            "esphome/esphome",
            "--json",
            "number,title,body,author,labels,url,state,mergedAt",
        )

        print("✓")
        return PullRequest.from_json(data)

    def cache_pr(self, pr: PullRequest) -> None:
        """Save PR to shared cache"""
        cache_file = self.prs_cache_dir / f"{pr.number}.json"
        with open(cache_file, "w") as f:
            json.dump(pr.to_json(), f, indent=2)

    def load_cached_pr(self, pr_number: int) -> PullRequest | None:
        """Load PR from shared cache if it exists"""
        cache_file = self.prs_cache_dir / f"{pr_number}.json"
        if not cache_file.exists():
            return None

        with open(cache_file) as f:
            data = json.load(f)
            return PullRequest(
                number=data["number"],
                title=data["title"],
                body=data["body"],
                author=data["author"],
                labels=data["labels"],
                url=data["url"],
                state=data["state"],
                merged_at=data.get("merged_at"),
            )

    def fetch_and_cache_prs(self, pr_numbers: list[int]) -> list[PullRequest]:
        """Fetch PRs and cache them locally"""
        prs = []

        for pr_number in pr_numbers:
            # Check cache first unless force update
            if not self.force_update:
                cached_pr = self.load_cached_pr(pr_number)
                if cached_pr:
                    print(f"Using cached PR #{pr_number}")
                    prs.append(cached_pr)
                    continue

            # Fetch from GitHub
            pr = self.fetch_pr(pr_number)
            self.cache_pr(pr)
            prs.append(pr)

        return prs

    def load_prs_by_numbers(self, pr_numbers: list[int]) -> list[PullRequest]:
        """Load specific PRs from shared cache by their numbers"""
        prs = []
        for pr_number in pr_numbers:
            pr = self.load_cached_pr(pr_number)
            if pr:
                prs.append(pr)
        return prs

    def generate_prompts(self, prs: list[PullRequest]) -> None:
        """Generate AI prompts for Claude"""
        print("\n=== Generating AI Prompts ===\n")

        # Group PRs by label
        breaking_changes = [pr for pr in prs if LABEL_BREAKING_CHANGE in pr.labels]
        new_features = [pr for pr in prs if LABEL_NEW_FEATURE in pr.labels]
        new_components = [pr for pr in prs if LABEL_NEW_COMPONENT in pr.labels]

        # Generate Combined Overview + Feature Highlights Prompt
        overview_and_highlights_prompt = self._generate_overview_and_highlights_prompt(
            prs, new_features, new_components, breaking_changes
        )
        overview_highlights_file = self.prompts_dir / "overview_and_highlights.txt"
        overview_highlights_file.write_text(overview_and_highlights_prompt)

        # Generate Combined Breaking Changes Prompt (user + developer)
        if breaking_changes:
            breaking_prompt = self._generate_combined_breaking_changes_prompt(
                breaking_changes
            )
            breaking_file = self.prompts_dir / "breaking_changes.txt"
            breaking_file.write_text(breaking_prompt)

        # Print instructions
        print("\n" + "=" * 80)
        print("STEP 1: Process prompts through Claude Code CLI")
        print("=" * 80)
        print("\nStart Claude Code CLI and read the prompt files:\n")
        print("  claude")
        print(f"  > Please read {overview_highlights_file} and follow the instructions")
        if breaking_changes:
            print(f"  > Please read {breaking_file} and follow the instructions")

        print("\nPrompt 1: Overview + Feature Highlights (COMBINED)")
        print(f"  Prompt: {overview_highlights_file}")
        print(f"  Outputs: {self.responses_dir / 'release_overview.md'}")
        print(f"           {self.responses_dir / 'feature_highlights.md'}")

        if breaking_changes:
            print("\nPrompt 2: Breaking Changes - Users + Developers (COMBINED)")
            print(f"  Prompt: {breaking_file}")
            print(f"  Outputs: {self.responses_dir / 'breaking_changes_users.md'}")
            print(f"           {self.responses_dir / 'breaking_changes_developers.md'}")

        print("\nNote: Each prompt will generate TWO output files automatically.")

        print("\n" + "=" * 80)
        print("STEP 2: Assemble the changelog")
        print("=" * 80)
        print(f"  python script/generate_release_notes.py {self.version} --assemble")

        print("\n" + "=" * 80)
        print("To reset and try again (delete AI responses):")
        print("=" * 80)
        print(f"  rm -rf {self.responses_dir}")
        print("  # Then re-run step 1 above")

        print("\n" + "=" * 80)
        print("STEP 3: REVIEW AND EDIT ASSEMBLED CHANGELOG (CRITICAL!)")
        print("=" * 80)
        print("\n⚠️  WARNING: AI-generated content MUST be reviewed for accuracy!")
        print("\nCarefully review and edit the assembled changelog:")
        print(f"  content/changelog/{self.version}.md")
        print("\nCheck for:")
        print("  ✓ Hallucinations or inaccurate technical claims")
        print(
            "  ✓ Incorrect compatibility statements (e.g., claiming breaking changes are backward compatible)"
        )
        print("  ✓ Mischaracterized features or incorrect measurements")
        print("  ✓ Proper tone and clarity")
        print("  ✓ Correct component links and formatting")
        print()

    def _generate_overview_and_highlights_prompt(
        self,
        all_prs: list[PullRequest],
        new_features: list[PullRequest],
        new_components: list[PullRequest],
        breaking_changes: list[PullRequest],
    ) -> str:
        """Generate combined prompt for release overview and feature highlights"""
        template = self.jinja_env.get_template("overview_and_highlights.txt")

        return template.render(
            version=str(self.version),
            overview_file=self.responses_dir / "release_overview.md",
            highlights_file=self.responses_dir / "feature_highlights.md",
            prs_cache_dir=self.prs_cache_dir,
            total_prs=len(all_prs),
            new_features=new_features,
            new_components=new_components,
            breaking_changes=breaking_changes,
        )

    def _generate_combined_breaking_changes_prompt(
        self, breaking_prs: list[PullRequest]
    ) -> str:
        """Generate combined prompt for both user and developer breaking changes"""
        template = self.jinja_env.get_template("breaking_changes.txt")

        return template.render(
            version=str(self.version),
            users_file=self.responses_dir / "breaking_changes_users.md",
            devs_file=self.responses_dir / "breaking_changes_developers.md",
            prs_cache_dir=self.prs_cache_dir,
            breaking_changes=breaking_prs,
        )

    def assemble_changelog(self) -> bool:
        """Assemble final changelog from template and AI responses"""
        print("\n=== Assembling Changelog ===\n")

        # Check that AI responses exist
        overview_file = self.responses_dir / "release_overview.md"
        if not overview_file.exists():
            print(f"Error: Missing AI response: {overview_file}")
            print("Please run the prompts through Claude first")
            return False

        # Load template
        template_file = Path("script/release_notes_template.md")
        if not template_file.exists():
            print(f"Error: Template not found: {template_file}")
            return False

        template = template_file.read_text()

        # Check if destination file exists and has content to preserve
        output_file = Path("content/changelog") / f"{self.version}.md"
        existing_imgtable = None
        existing_full_list = None
        if output_file.exists():
            existing_content = output_file.read_text()

            # Extract existing imgtable content
            imgtable_match = re.search(
                r"{{< imgtable >}}(.*?){{< /imgtable >}}", existing_content, re.DOTALL
            )
            if imgtable_match and imgtable_match.group(1).strip():
                existing_imgtable = imgtable_match.group(0)
                print("✓ Preserving existing imgtable")

            # Extract existing "Full list of changes" section
            # This regex matches from "## Full list of changes" to end of file
            full_list_match = re.search(
                r"## Full list of changes.*?(?=^## |\Z)",
                existing_content,
                re.DOTALL | re.MULTILINE,
            )
            if full_list_match:
                existing_full_list = full_list_match.group(0)
                print("✓ Preserving existing 'Full list of changes' section")

        # Load AI responses
        overview = overview_file.read_text().strip()

        breaking_users_file = self.responses_dir / "breaking_changes_users.md"
        breaking_users = ""
        if breaking_users_file.exists():
            breaking_users = breaking_users_file.read_text().strip()

        breaking_devs_file = self.responses_dir / "breaking_changes_developers.md"
        breaking_devs = ""
        if breaking_devs_file.exists():
            breaking_devs = breaking_devs_file.read_text().strip()

        highlights_file = self.responses_dir / "feature_highlights.md"
        highlights = ""
        if highlights_file.exists():
            highlights = highlights_file.read_text().strip()

        # Load the PR numbers for this version from a manifest file
        manifest_file = self.version_dir / "pr_numbers.txt"
        if not manifest_file.exists():
            print(f"Error: PR manifest not found: {manifest_file}")
            print("Run without --assemble first to discover PRs")
            return False

        pr_numbers = [
            int(line.strip())
            for line in manifest_file.read_text().strip().split("\n")
            if line.strip()
        ]
        prs = self.load_prs_by_numbers(pr_numbers)

        if not prs:
            print("Error: No cached PRs found. Run without --assemble first")
            return False

        print(f"Loaded {len(prs)} PRs from cache")

        # Replace AI-generated sections
        template = self._replace_marker_content(template, "RELEASE_OVERVIEW", overview)

        if highlights:
            template = self._replace_marker_content(
                template, "FEATURE_HIGHLIGHTS", highlights
            )

        if breaking_users:
            template = self._replace_marker_content(
                template, "BREAKING_CHANGES_USERS", breaking_users
            )

        if breaking_devs:
            template = self._replace_marker_content(
                template, "BREAKING_CHANGES_DEVELOPERS", breaking_devs
            )

        # Generate auto sections
        template = self._generate_auto_sections(template, prs)

        # Replace version placeholders
        template = self._replace_placeholders(template)

        # Replace imgtable if we have one preserved
        if existing_imgtable:
            template = re.sub(
                r"<!-- MANUAL: Add featured components here -->\s*{{< imgtable >}}.*?{{< /imgtable >}}",
                existing_imgtable,
                template,
                flags=re.DOTALL,
            )

        # Replace "Full list of changes" section if we have one preserved
        if existing_full_list:
            template = re.sub(
                r"## Full list of changes.*?(?=^## |\Z)",
                existing_full_list,
                template,
                flags=re.DOTALL | re.MULTILINE,
            )

        # Write output

        if self.dry_run:
            print("\n" + "=" * 80)
            print("DRY RUN - Would write to:", output_file)
            print("=" * 80)
            print(template[:1000])  # Show first 1000 chars
            print("...")
        else:
            output_file.parent.mkdir(parents=True, exist_ok=True)
            output_file.write_text(template)
            print(f"\n✓ Changelog written to: {output_file}")

        return True

    def _replace_marker_content(self, template: str, marker: str, content: str) -> str:
        """Replace content between <!-- MARKER_START --> and <!-- MARKER_END -->"""
        pattern = f"<!-- {marker}_START -->.*?<!-- {marker}_END -->"
        replacement = f"<!-- {marker}_START -->\n{content}\n<!-- {marker}_END -->"

        result, count = re.subn(pattern, replacement, template, flags=re.DOTALL)

        if count == 0:
            print(f"Warning: Marker {marker} not found in template")
        else:
            print(f"✓ Replaced {marker}")

        return result

    def _generate_auto_sections(self, template: str, prs: list[PullRequest]) -> str:
        """Generate auto-populated sections from PR data"""
        # Group PRs by label
        new_features = [pr for pr in prs if "new-feature" in pr.labels]
        new_components = [pr for pr in prs if "new-component" in pr.labels]
        breaking_changes = [pr for pr in prs if "breaking-change" in pr.labels]

        # Generate lists
        features_list = self._format_pr_list(new_features)
        components_list = self._format_pr_list(new_components)
        breaking_list = self._format_pr_list(breaking_changes)
        all_list = self._format_pr_list(prs)

        # Replace sections
        template = self._replace_marker_content(
            template, "AUTO_GENERATED_NEW_FEATURES", features_list
        )
        template = self._replace_marker_content(
            template, "AUTO_GENERATED_NEW_COMPONENTS", components_list
        )
        template = self._replace_marker_content(
            template, "AUTO_GENERATED_BREAKING_CHANGES_LIST", breaking_list
        )
        return self._replace_marker_content(
            template, "AUTO_GENERATED_ALL_CHANGES", all_list
        )

    def _format_pr_list(self, prs: list[PullRequest]) -> str:
        """Format PRs as markdown list"""
        if not prs:
            return "None"

        lines = []
        for pr in prs:
            # Extract component from title if present [component]
            match = re.match(r"\[([^\]]+)\]\s*(.*)", pr.title)
            if match:
                component = match.group(1)
                title = match.group(2)
            else:
                component = ""
                title = pr.title

            # Format: - [component] Description [esphome#1234](url) by [@author](url)
            author_url = f"https://github.com/{pr.author}"
            pr_url = pr.url.replace("api.github.com/repos", "github.com")

            if component:
                line = f"- [{component}] {title} [esphome#{pr.number}]({pr_url}) by [@{pr.author}]({author_url})"
            else:
                line = f"- {title} [esphome#{pr.number}]({pr_url}) by [@{pr.author}]({author_url})"

            lines.append(line)

        return "\n".join(lines)

    def _replace_placeholders(self, template: str) -> str:
        """Replace version placeholders"""
        # Format date
        now = datetime.now()
        date_str = now.strftime('%B %Y')

        template = template.replace("{VERSION}", str(self.version))
        template = template.replace("{DATE}", date_str)

        print(f"✓ Replaced placeholders: {self.version}, {date_str}")

        return template

    def run(self, assemble_only: bool = False) -> bool:
        """Main workflow"""
        self.ensure_dirs()

        if assemble_only:
            # Skip PR discovery, just assemble from cached data
            return self.assemble_changelog()

        # Discover and fetch PRs
        pr_numbers = self.discover_prs()

        if not pr_numbers:
            print("\nWarning: No PRs found!")
            print("This might mean:")
            print("  1. The version tags are incorrect")
            print("  2. No PRs have been merged since the last release")
            print("  3. There's an issue with the GitHub API")
            return False

        print(f"\nFound {len(pr_numbers)} PRs")

        # Fetch and cache
        print("\n=== Fetching PR Metadata ===\n")
        prs = self.fetch_and_cache_prs(pr_numbers)
        print(f"\n✓ Cached {len(prs)} PRs to {self.prs_cache_dir}")

        # Save PR numbers manifest for this version
        manifest_file = self.version_dir / "pr_numbers.txt"
        manifest_file.write_text("\n".join(str(n) for n in pr_numbers) + "\n")
        print(f"✓ Saved PR manifest to {manifest_file}")

        # Generate prompts
        self.generate_prompts(prs)

        return True


def main() -> int:
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Generate ESPHome release notes",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Discover PRs and generate prompts
  python script/generate_release_notes.py 2025.11.0

  # Force re-fetch all PRs from GitHub
  python script/generate_release_notes.py 2025.11.0 --update

  # Assemble changelog from AI responses (skip PR discovery)
  python script/generate_release_notes.py 2025.11.0 --assemble

  # Dry run (show what would be generated)
  python script/generate_release_notes.py 2025.11.0 --assemble --dry-run
        """,
    )
    parser.add_argument(
        "version", type=str, help="Version to generate notes for (e.g., 2025.11.0)"
    )
    parser.add_argument(
        "--update",
        action="store_true",
        help="Force re-fetch all PRs from GitHub (ignore cache)",
    )
    parser.add_argument(
        "--assemble",
        action="store_true",
        help="Skip PR discovery, assemble changelog from cached AI responses",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be generated without writing files",
    )

    args = parser.parse_args()

    try:
        version = Version.parse(args.version)
    except ValueError as e:
        print(f"Error: {e}")
        return 1

    generator = ReleaseNotesGenerator(
        version=version,
        force_update=args.update,
        dry_run=args.dry_run,
    )

    # Check GitHub CLI is installed and authenticated
    generator.check_github_cli()

    success = generator.run(assemble_only=args.assemble)
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
