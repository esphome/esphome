#!/usr/bin/env python3

import argparse
import collections
from pathlib import Path
from typing import Optional
import colorama
import fnmatch
import functools
import os.path
import re
import sys
import os
import subprocess

try:
    from PIL import Image

    PILLOW_INSTALLED = True
except ImportError:
    print("Pillow could not be imported - will not run image checks")
    print("Install pillow with `pip3 install pillow`")
    PILLOW_INSTALLED = False


class AnsiFore:
    KEEP = ""
    BLACK = "\033[30m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    WHITE = "\033[37m"
    RESET = "\033[39m"

    BOLD_BLACK = "\033[1;30m"
    BOLD_RED = "\033[1;31m"
    BOLD_GREEN = "\033[1;32m"
    BOLD_YELLOW = "\033[1;33m"
    BOLD_BLUE = "\033[1;34m"
    BOLD_MAGENTA = "\033[1;35m"
    BOLD_CYAN = "\033[1;36m"
    BOLD_WHITE = "\033[1;37m"
    BOLD_RESET = "\033[1;39m"


class AnsiStyle:
    BRIGHT = "\033[1m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    THIN = "\033[2m"
    NORMAL = "\033[22m"
    RESET_ALL = "\033[0m"


Fore = AnsiFore()
Style = AnsiStyle()


def print_error_for_file(file: str, body: Optional[str]):
    print(f"{AnsiFore.GREEN}### File {AnsiStyle.BRIGHT}{file}{AnsiStyle.RESET_ALL}")
    print()
    if body is not None:
        print(body)
        print()


def git_ls_files(patterns=None):
    command = ["git", "ls-files", "-s"]
    if patterns is not None:
        command.extend(patterns)
    proc = subprocess.Popen(command, stdout=subprocess.PIPE)
    output, _ = proc.communicate()
    lines = [x.split() for x in output.decode("utf-8").splitlines()]
    return {s[3].strip(): int(s[0]) for s in lines}


def find_all(a_str, sub):
    if not a_str.find(sub):
        # Optimization: If str is not in whole text, then do not try
        # on each line
        return
    for i, line in enumerate(a_str.split("\n")):
        column = 0
        while True:
            column = line.find(sub, column)
            if column == -1:
                break
            yield i, column
            column += len(sub)


colorama.init()

parser = argparse.ArgumentParser()
parser.add_argument(
    "files", nargs="*", default=[], help="files to be processed (regex on path)"
)
args = parser.parse_args()

EXECUTABLE_BIT = git_ls_files()
files = list(EXECUTABLE_BIT.keys())
# Match against re
file_name_re = re.compile("|".join(args.files))
files = [p for p in files if file_name_re.search(p)]

files.sort()

file_types = (
    ".cfg",
    ".css",
    ".gif",
    ".h",
    ".html",
    ".ico",
    ".jpg",
    ".js",
    ".json",
    ".md",
    ".png",
    ".py",
    ".svg",
    ".toml",
    ".txt",
    ".webmanifest",
    ".xml",
    ".yaml",
    ".yml",
    "",
)
docs_types = [".md"]
image_types = [".webp", ".jpg", ".ico", ".png", ".svg", ".gif"]

LINT_FILE_CHECKS = []
LINT_CONTENT_CHECKS = []
LINT_POST_CHECKS = []


def run_check(lint_obj, fname, *args):
    include = lint_obj["include"]
    exclude = lint_obj["exclude"]
    func = lint_obj["func"]
    if include is not None:
        for incl in include:
            if fnmatch.fnmatch(fname, incl):
                break
        else:
            return None
    for excl in exclude:
        if fnmatch.fnmatch(fname, excl):
            return None
    return func(*args)


def run_checks(lints, fname, *args):
    for lint in lints:
        try:
            add_errors(fname, run_check(lint, fname, *args))
        except Exception:
            print(f"Check {lint['func'].__name__} on file {fname} failed:")
            raise


def _add_check(checks, func, include=None, exclude=None):
    checks.append(
        {
            "include": include,
            "exclude": exclude or [],
            "func": func,
        }
    )


def lint_file_check(**kwargs):
    def decorator(func):
        _add_check(LINT_FILE_CHECKS, func, **kwargs)
        return func

    return decorator


def lint_content_check(**kwargs):
    def decorator(func):
        _add_check(LINT_CONTENT_CHECKS, func, **kwargs)
        return func

    return decorator


def lint_post_check(func):
    _add_check(LINT_POST_CHECKS, func)
    return func


def lint_re_check(regex, **kwargs):
    flags = kwargs.pop("flags", re.MULTILINE)
    prog = re.compile(regex, flags)
    decor = lint_content_check(**kwargs)

    def decorator(func):
        @functools.wraps(func)
        def new_func(fname, content):
            errors = []
            for match in prog.finditer(content):
                if "NOLINT" in match.group(0):
                    continue
                lineno = content.count("\n", 0, match.start()) + 1
                substr = content[: match.start()]
                col = len(substr) - substr.rfind("\n")
                err = func(fname, match)
                if err is None:
                    continue
                errors.append((lineno, col + 1, err))
            return errors

        return decor(new_func)

    return decorator


def lint_content_find_check(find, only_first=False, **kwargs):
    decor = lint_content_check(**kwargs)

    def decorator(func):
        @functools.wraps(func)
        def new_func(fname, content):
            find_ = find
            if callable(find):
                find_ = find(fname, content)
            errors = []
            for line, col in find_all(content, find_):
                err = func(fname)
                errors.append((line + 1, col + 1, err))
                if only_first:
                    break
            return errors

        return decor(new_func)

    return decorator


@lint_file_check(exclude=[f"*{f}" for f in file_types])
def lint_ext_check(fname: str, stat: os.stat_result):
    return (
        "This file extension is not a registered file type. If this is an error, please "
        "update the script/ci-custom.py script."
    )


@lint_file_check(exclude=["script/*", ".devcontainer/*", "lint.py"])
def lint_executable_bit(fname: str, stat: os.stat_result):
    ex = EXECUTABLE_BIT[fname]
    if ex != 100644:
        return (
            "File has invalid executable bit {}. If running from a windows machine please "
            "see disabling executable bit in git.".format(ex)
        )
    return None


@lint_file_check(
    include=[f"images/*{ext}" for ext in image_types], exclude=["images/hero.png"]
)
def lint_index_images_size(fname: str, stat: os.stat_result):
    if stat.st_size > 40 * 1024:
        return (
            "Image is too large. The files in the images/ folder are displayed on esphome's "
            "front page and thus should be small (no more than 300x300px, and <40kb). "
            "Use a tool like https://compress-or-die.com/ to reduce the image size. "
            f"Size of file: {stat.st_size / 1024:.0f}kb"
        )
    return None


@lint_file_check(include=[f"*{ext}" for ext in image_types])
def lint_all_images_size(fname: str, stat: os.stat_result):
    if stat.st_size > 1024 * 1024:
        return (
            "Image is too large. Images in ESPHome's codebase should be 1MB in size max. "
            "Use a tool like https://compress-or-die.com/ to reduce the image size. "
            f"Size of file: {stat.st_size / 1024:.0f}kb"
        )
    return None


if PILLOW_INSTALLED:

    @lint_file_check(
        include=["images/*.jpg", "images/*.png"], exclude=["images/hero.png"]
    )
    def lint_index_images_dimensions(fname: str, stat: os.stat_result):
        img = Image.open(fname)
        if img.width > 300 or img.height > 300:
            return (
                "Image has too large dimensions. The images in the images/ folder are displayed on "
                "ESPHome's main page, so need to be lightweight. We allow a max of 300x300 for images on this page. "
                "Use a tool like https://compress-or-die.com/ to reduce the image size. "
                f"Dimensions of this image: {img.width}x{img.height}"
            )
        return None


@lint_content_find_check(
    "\t",
    only_first=True,
    exclude=[
        "Makefile",
    ],
)
def lint_tabs(fname):
    return "File contains tab character. Please convert tabs to spaces."


@lint_content_find_check("\r", only_first=True)
def lint_newline(fname):
    return "File contains Windows newline. Please set your editor to Unix newline mode."


@lint_content_check(exclude=["*.svg", "runtime.txt", "_static/*"])
def lint_end_newline(fname, content):
    if content and not content.endswith("\n"):
        return "File does not end with a newline, please add an empty line at the end of the file."
    return None


@lint_re_check(
    r"\[([^\]]+)\]\((https://esphome\.io/[^)]+)\)",
    include=["*.md"],
)
def lint_esphome_io_link(fname, match):
    link_text = match.group(1)
    full_url = match.group(2)
    return (
        f"Markdown link to esphome.io should use relative path. "
        f"Change [{link_text}]({full_url}) to use a relative URL"
    )


# Build cache of all anchors in the documentation
ANCHOR_CACHE = None


def build_anchor_cache():
    """Build a cache of all anchors (headings and shortcodes) in markdown files."""
    global ANCHOR_CACHE
    if ANCHOR_CACHE is not None:
        return ANCHOR_CACHE

    ANCHOR_CACHE = {}
    content_dir = Path("content")
    if not content_dir.exists():
        return ANCHOR_CACHE

    def generate_slug(text):
        """Generate Hugo-compatible slug from heading text."""
        slug = re.sub(r"[^\w\s-]", "", text).lower()
        slug = re.sub(r"[-\s]+", "-", slug).strip("-")
        return slug

    for md_file in content_dir.rglob("*.md"):
        rel_path = md_file.relative_to(content_dir)
        # Convert file path to page path
        if md_file.name == "_index.md":
            if str(rel_path) == "_index.md":
                page_path = ""  # Root index
            else:
                page_path = str(rel_path.parent)
        else:
            page_path = str(rel_path.with_suffix(""))

        page_path = page_path.replace("\\", "/")
        anchors = set()

        try:
            with open(md_file, "r", encoding="utf-8") as f:
                content = f.read()
                lines = content.split("\n")

            for line in lines:
                # Match headings
                heading_match = re.match(r"^(#{1,6})\s+(.*)", line)
                if heading_match:
                    heading_text = heading_match.group(2).strip()
                    anchor_id = generate_slug(heading_text)
                    anchors.add(anchor_id)

                # Match anchor shortcodes
                shortcode_match = re.search(r'\{\{<\s*anchor\s+"([^\s>]+)"\s*>}}', line)
                if shortcode_match:
                    anchor_id = shortcode_match.group(1)
                    anchors.add(anchor_id)

            ANCHOR_CACHE[page_path] = anchors
        except Exception:
            # Skip files that can't be read
            pass

    return ANCHOR_CACHE


@lint_content_check(include=["*.md"])
def lint_internal_links(fname, content):
    """Validate internal markdown links."""
    errors = []
    build_anchor_cache()

    # Match markdown links: [text](url)
    # Capture groups: text and URL
    link_pattern = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")

    for match in link_pattern.finditer(content):
        link_text = match.group(1)
        link_url = match.group(2)
        lineno = content.count("\n", 0, match.start()) + 1
        col = match.start() - content.rfind("\n", 0, match.start())

        # Skip external links (http://, https://, mailto:, etc.)
        if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", link_url):
            continue

        # Check anchor-only links (these used to be resolved by the old system)
        if link_url.startswith("#"):
            anchor_id = link_url.lstrip("#")

            # Get current page path (only for files in content/)
            current_file = Path(fname)
            try:
                if current_file.name == "_index.md":
                    if str(current_file.parent) == "content":
                        current_page = ""
                    else:
                        current_page = str(current_file.parent.relative_to("content"))
                else:
                    current_page = str(current_file.relative_to("content").with_suffix(""))

                current_page = current_page.replace("\\", "/")

                # Check if anchor exists on current page
                if current_page in ANCHOR_CACHE:
                    if anchor_id not in ANCHOR_CACHE[current_page]:
                        # Anchor doesn't exist on current page - likely a broken cross-page link
                        errors.append(
                            (
                                lineno,
                                col,
                                f"Anchor-only link '#{anchor_id}' not found on current page. "
                                f"If this should link to another page, use the full path like "
                                f"[{link_text}](/path/to/page#{anchor_id})",
                            )
                        )
            except ValueError:
                # File is not in content/ directory, skip anchor validation
                pass
            continue

        # Skip relative links to static assets (images, etc.)
        # These don't start with / and have file extensions
        if not link_url.startswith("/") and re.search(r"\.(png|jpg|jpeg|gif|svg|webp|pdf|zip)$", link_url, re.IGNORECASE):
            continue

        # Skip links that look like code/lambda parameters (contain spaces, parentheses, etc.)
        if " " in link_url or "(" in link_url or ")" in link_url:
            continue

        # Skip relative links without leading slash (not documentation pages)
        # unless they're in the components directory
        if not link_url.startswith("/"):
            # Allow relative component links
            if not ("components/" in fname and not re.search(r"\.", link_url)):
                continue

        # Parse internal link
        if "#" in link_url:
            path_part, anchor_part = link_url.split("#", 1)
        else:
            path_part = link_url
            anchor_part = None

        # Validate path - must start with /
        if not path_part.startswith("/"):
            continue

        # Remove leading slash
        clean_path = path_part.lstrip("/")

        # Skip paths with query strings or fragments that look like URLs
        if "?" in clean_path or ".html" in clean_path:
            continue

        # Check if page exists
        if clean_path not in ANCHOR_CACHE:
            # Try with _index
            if clean_path and not clean_path.endswith("/"):
                clean_path_index = clean_path
            else:
                clean_path_index = clean_path.rstrip("/")

            if clean_path_index not in ANCHOR_CACHE:
                errors.append(
                    (
                        lineno,
                        col,
                        f"Internal link references non-existent page: '{path_part}' in link [{link_text}]({link_url})",
                    )
                )
                continue

        # Validate anchor if present
        if anchor_part:
            target_page = clean_path
            if target_page not in ANCHOR_CACHE:
                target_page = clean_path.rstrip("/")

            if target_page in ANCHOR_CACHE:
                if anchor_part not in ANCHOR_CACHE[target_page]:
                    errors.append(
                        (
                            lineno,
                            col,
                            f"Internal link references non-existent anchor: '#{anchor_part}' on page '{path_part}' in link [{link_text}]({link_url})",
                        )
                    )

    return errors


def highlight(s):
    return f"\033[36m{s}\033[0m"


errors = collections.defaultdict(list)


def add_errors(fname, errs):
    if not isinstance(errs, list):
        errs = [errs]
    for err in errs:
        if err is None:
            continue
        try:
            lineno, col, msg = err
        except ValueError:
            lineno = 1
            col = 1
            msg = err
        if not isinstance(msg, str):
            raise ValueError("Error is not instance of string!")
        if not isinstance(lineno, int):
            raise ValueError("Line number is not an int!")
        if not isinstance(col, int):
            raise ValueError("Column number is not an int!")
        errors[fname].append((lineno, col, msg))


for fname in files:
    p = Path(fname)
    if not p.is_file():
        # file deleted but in git index
        continue
    run_checks(LINT_FILE_CHECKS, fname, fname, p.stat())
    if p.suffix in image_types:
        continue
    try:
        with open(fname, "r") as f_handle:
            content = f_handle.read()
    except UnicodeDecodeError:
        add_errors(
            fname,
            "File is not readable as UTF-8. Please set your editor to UTF-8 mode.",
        )
        continue
    run_checks(LINT_CONTENT_CHECKS, fname, fname, content)

run_checks(LINT_POST_CHECKS, "POST")

for f, errs in sorted(errors.items()):
    err_str = (
        f"{AnsiStyle.BOLD}{f}:{lineno}:{col}:{AnsiStyle.RESET_ALL} "
        f"{AnsiStyle.BOLD}{AnsiFore.RED}{'lint:'}{AnsiStyle.RESET_ALL} {msg}\n"
        for lineno, col, msg in errs
    )
    print_error_for_file(f, "\n".join(err_str))

sys.exit(len(errors))
