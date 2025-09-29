#!/usr/bin/env python3
"""
Convert Sphinx RST files to Hugo Markdown format.
This script helps migrate the ESPHome documentation from Sphinx to Hugo.
"""

import argparse
import csv
import io
import os
import re
import shutil
from itertools import zip_longest

from git import Repo

# Global anchor map to store all anchors and their document paths
anchor_map = {}


# Global variables for image tracking
class ImageInfo:
    def __init__(self, name, path, source):
        self.name = name
        self.path = path
        self.source = source
        self.count = 0

    def increment(self):
        self.count += 1


image_map = {}
included_files = set()
substitutions = {}


def find_included_files(file_path):
    """
    Parse a file for include directives and return a list of included files.

    Args:
        file_path: Path to the file to parse

    Returns:
        List of absolute paths to included files
    """
    include_file_list = []

    try:
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Look for .. include:: directives
        include_pattern = r"^\s*\.\.\s+include::\s+(.+?)$"
        matches = re.finditer(include_pattern, content, re.MULTILINE)

        for match in matches:
            included_path = match.group(1).strip()

            # Handle relative paths
            if not os.path.isabs(included_path):
                included_path = os.path.normpath(
                    os.path.join(os.path.dirname(file_path), included_path)
                )

            # Check if the file exists
            if os.path.exists(included_path):
                include_file_list.append(included_path)
            else:
                print(f"Warning: Included file not found: {included_path}")

        return include_file_list

    except Exception as e:
        print(f"Error parsing includes in {file_path}: {e}")
        return []


def get_all_included_files(input_dir):
    for root, _, files in os.walk(input_dir):
        for file in files:
            if file.endswith(".rst"):
                included_files.update(
                    set(find_included_files(os.path.join(root, file)))
                )


def build_anchor_map(input_dir):
    """Scan all RST files and build a map of anchors to their document paths."""
    print("Building anchor map...")

    for root, _, files in os.walk(input_dir):
        for file in files:
            file_path = str(os.path.join(root, file))
            if file_path not in included_files and file.endswith(".rst"):
                rel_path = os.path.relpath(file_path, input_dir)
                doc_path = os.path.splitext(rel_path)[0]

                # Convert to Hugo path format
                doc_path = doc_path.lower()

                # Handle index files
                if os.path.basename(doc_path) == "index":
                    doc_path = os.path.dirname(doc_path) + "/_index"

                rel_path, rst_content = get_rst_content(input_dir, file_path)
                for i, line in enumerate(rst_content):
                    if line.startswith(".. _") and line.endswith(":"):
                        anchor_name = line[
                            4:-1
                        ]  # Extract the anchor name without the '.. _' prefix and ':' suffix
                        if i < len(rst_content) - 2:
                            text = rst_content[i + 2].strip()
                        else:
                            text = ""
                        anchor_map[anchor_name] = doc_path, text

    print(f"Found {len(anchor_map)} anchors across all documents")


def normalize_csv_lines(lines, delimiter=","):
    # Read from list of lines using StringIO
    reader = csv.reader(
        io.StringIO("\n".join(lines)),
        delimiter=delimiter,
        quotechar='"',
        skipinitialspace=True,
    )
    rows = list(reader)

    # Find the max number of columns
    max_columns = max(len(row) for row in rows)

    # Pad rows with missing columns
    normalized_rows = [row + [""] * (max_columns - len(row)) for row in rows]
    for row in normalized_rows:
        if len(row) > 2:
            row[2] = row[2].replace(".avif", ".webp")

    return normalized_rows


def get_indented_block(lines, i, current_indent):
    # skip blank lines
    while i < len(lines) and not lines[i].strip():
        i += 1

    md_lines = []
    # Determine the indentation level of the code block content
    code_indent = 0
    if i < len(lines) and lines[i].startswith(" "):
        code_indent = len(lines[i]) - len(lines[i].lstrip(" "))

    # Add code content
    while i < len(lines):
        line = lines[i]
        this_indent = min(len(line) - len(line.lstrip()), code_indent)
        if not line:
            md_lines.append("")
            i += 1
        elif this_indent and this_indent >= code_indent - 1:
            # Remove only the code block indentation, preserve any existing indentation
            if this_indent >= code_indent:
                line = line[this_indent:]
            else:
                line = line[this_indent - 1 :]
            md_lines.append(" " * current_indent + line)
            i += 1
        else:  # Line without expected indentation - end of code block
            break
    return i, md_lines


bullet_regex = re.compile(r"^(\s*)([-*+])\s+")


def get_heading(lines, index, heading_underlines):
    while index < len(lines):
        line = lines[index]
        if line.strip() == "":
            index += 1
            continue
        if line and index + 1 < len(lines) and len(lines[index + 1]) >= len(line):
            next_line = lines[index + 1]
            uchar = next_line[0]
            if uchar in heading_underlines and all(x == uchar for x in next_line):
                level = heading_underlines.index(uchar) + 1
                return level, index + 2, line.strip()
        break
    return None, None, None


def rst_unicode_to_markdown(text):
    # Capture: optional text, optional whitespace, and one or more 0xAB sequences
    pattern = re.compile(r"unicode::\s*([^\d]*?)?\s*((?:0x[0-9A-Fa-f]+(?:\s+)?)+)")

    def repl(match):
        literal_text = (match.group(1) or "").rstrip()
        codepoints = match.group(2).split()
        chars = "".join(chr(int(cp, 16)) for cp in codepoints)
        return f"{literal_text}{chars}"

    return pattern.sub(repl, text)


def convert_rst_to_md(lines, filename):
    """Convert RST content to Markdown using line-by-line processing."""
    heading_underlines = []
    footnotes = []  # Store footnotes to add at the end
    seo = {}
    indent_stack = []

    # Extract title and SEO data
    title = ""
    explicit_title = ""
    skip_build = False

    def process_lines(inner_lines):
        result_lines = []
        current_idx = 0

        while current_idx < len(inner_lines):
            this_line = inner_lines[current_idx]
            if this_line and not this_line[0].isspace():
                indent_stack.clear()

            current_indent = len(this_line) - len(this_line.lstrip())
            # Skip title directive
            if this_line.startswith(".. title::"):
                current_idx += 1
                continue

            if this_line.startswith(".. seo::"):
                current_idx, seo_lines = get_indented_block(
                    inner_lines, current_idx + 1, 0
                )
                for this_line in seo_lines:
                    if this_line.strip().startswith(":description:"):
                        seo["description"] = this_line.split(":")[2].strip()
                    if this_line.strip().startswith(":image:"):
                        seo["image"] = this_line.split(":")[2].strip()
                continue

            if this_line.startswith(".. option::"):
                text = this_line.replace(".. option::", "").strip()
                current_idx += 1
                while (
                    current_idx < len(inner_lines)
                    and not inner_lines[current_idx].strip()
                ):
                    current_idx += 1
                result_lines.append(f'{{{{< option "{text}" >}}}}')
                while current_idx < len(inner_lines):
                    if not inner_lines[current_idx]:
                        result_lines.append("")
                        current_idx += 1
                        continue
                    if inner_lines[current_idx].startswith(" "):
                        result_lines.append(inner_lines[current_idx].strip())
                        current_idx += 1
                    else:
                        break
                result_lines.append("{{< /option >}}")
                continue

            # Handle imgtable directive
            if this_line.strip() == ".. imgtable::":
                current_idx += 1
                # Skip empty lines
                while (
                    current_idx < len(inner_lines)
                    and not inner_lines[current_idx].strip()
                ):
                    current_idx += 1

                # Start the imgtable shortcode
                result_lines.append("{{< imgtable >}}")
                csv_lines = []
                # Process each entry (each line should be indented)
                while current_idx < len(inner_lines):
                    current_line = inner_lines[current_idx].strip()

                    # If we hit an empty line or a non-indented line, we're done with this imgtable
                    if not inner_lines[current_idx].startswith("    ") and current_line:
                        break

                    # Skip empty lines within the imgtable
                    if not current_line or current_line.startswith(":"):
                        current_idx += 1
                        continue

                    # Process the entry - format is typically: Title, Link, Image, [Description]
                    csv_lines.append(current_line)
                    current_idx += 1

                csv_lines = normalize_csv_lines(csv_lines)
                for row in csv_lines:
                    result_lines.append(
                        ",".join(
                            '"'
                            + col.strip().replace('"', '""').replace(":", " -")
                            + '"'
                            for col in row
                        )
                    )
                # Close the imgtable shortcode
                result_lines.append("{{< /imgtable >}}")
                continue

            if this_line.startswith(".. program::"):
                current_idx += 1
                continue

            # Skip title (we'll add it later with frontmatter)
            if (
                this_line == title
                and current_idx + 1 < len(inner_lines)
                and re.match(r"^=+$", inner_lines[current_idx + 1])
            ):
                current_idx += 2
                continue

            # Handle existing anchor shortcodes
            anchor_name = None
            if re.match(r'^\{{<\s+anchor\s+["\'](\w+)["\']\s+>}}', this_line):
                anchor_name = re.match(
                    r'^\{{<\s+anchor\s+["\'](\w+)["\']\s+>}}', this_line
                ).group(1)
            # Handle RST anchors (.. _anchor:)
            elif this_line.startswith(".. _") and this_line.endswith(":"):
                anchor_name = this_line[
                    4:-1
                ]  # Extract the anchor name without the '.. _' prefix and ':' suffix
            if anchor_name:
                current_idx += 1
                level, next_index, heading = get_heading(
                    inner_lines, current_idx, heading_underlines
                )
                if heading:
                    heading = heading.lower().replace(" ", "-")
                if heading != anchor_name.lower():
                    result_lines.append(f'{{{{< anchor "{anchor_name.lower()}" >}}}}')
                continue

            # Handle raw HTML blocks that might contain buttons
            if this_line.strip().startswith(".. raw:: html"):
                button_lines, new_i = process_raw_html_block(inner_lines, current_idx)
                result_lines.extend(button_lines)
                current_idx = new_i
                continue

            # Handle list-table directive
            if this_line.strip().startswith(
                ".. list-table::"
            ) or this_line.strip().startswith("..  list-table::"):
                table_lines, new_i = process_list_table(inner_lines, current_idx)
                result_lines.extend(table_lines)
                current_idx = new_i
                continue

            # Handle csv-table directive
            if this_line.strip().startswith(".. csv-table::"):
                table_lines, new_i = process_csv_table(inner_lines, current_idx)
                result_lines.extend(table_lines)
                current_idx = new_i
                continue

            # Handle headings
            if (
                this_line
                and current_idx + 1 < len(inner_lines)
                and len(inner_lines[current_idx + 1]) >= len(this_line)
            ):
                next_line = inner_lines[current_idx + 1]
                uchar = next_line[0]
                if uchar in heading_underlines and all(x == uchar for x in next_line):
                    level = heading_underlines.index(uchar) + 1
                    prefix = "#" * level
                    result_lines.append(f"{prefix} {this_line}")
                    current_idx += 2
                    continue

            # Handle code blocks
            if match := re.match(
                r"^(\s*)(-\s)?\.\.\s+(code-block|code)::\s*(\w*)", this_line
            ):
                # Extract language
                language = match.group(4)
                prefix = match.group(2) or ""
                leading_space = match.group(1) or ""
                indent = len(prefix) + len(leading_space)

                # Add the code block start with proper indentation
                result_lines.append(prefix + f"```{language}")

                current_idx, new_lines = get_indented_block(
                    inner_lines, current_idx + 1, current_indent + indent
                )
                while len(new_lines) and not new_lines[-1].strip():
                    new_lines.pop()
                result_lines.extend(new_lines)
                result_lines.append("")
                result_lines.append(" " * len(prefix) + "```")
                continue

            if this_line.lstrip().startswith(".. math::"):
                # Get the indentation of the current line

                # Add the code block start with proper indentation
                result_lines.append(" " * current_indent + "{{< math >}}")

                current_idx, new_lines = get_indented_block(
                    inner_lines, current_idx + 1, current_indent
                )
                result_lines.extend(new_lines)
                result_lines.append(" " * current_indent + "{{< /math >}}")
                continue

            if this_line.lstrip().startswith(".. collapse::"):
                # Get the indentation of the current line
                collapse_title = this_line.strip().removeprefix(".. collapse::").strip()

                # Add the code block start with proper indentation

                is_open = False
                current_idx, new_lines = get_indented_block(
                    inner_lines, current_idx + 1, current_indent
                )
                if new_lines[0].startswith(":open:"):
                    is_open = True
                    new_lines = new_lines[1:]
                result_lines.append(
                    " " * current_indent
                    + f'{{{{< collapse "{collapse_title}" {is_open} >}}}}'
                )
                result_lines.extend(new_lines)
                result_lines.append(" " * current_indent + "{{< /collapse >}}")
                continue

            # Handle notes
            handled = False
            for directive in ["note", "warning", "caution", "important", "tip"]:
                if this_line.strip().startswith(f".. {directive}::"):
                    if not this_line.strip().endswith(f".. {directive}::"):
                        result_lines.append(f"{{{{< {directive} >}}}}")
                        result_lines.append(
                            this_line.removeprefix(f".. {directive}::").strip()
                        )
                        result_lines.append(f"{{{{< /{directive} >}}}}")
                        current_idx += 1
                    else:
                        # Get the indentation level of the note directive
                        result_lines.append(f"{{{{< {directive} >}}}}")
                        current_idx, note_lines = get_indented_block(
                            inner_lines, current_idx + 1, current_indent
                        )
                        note_lines = process_lines(note_lines)
                        # Note blocks can't be indented.
                        result_lines.extend(x[current_indent:] for x in note_lines)
                        result_lines.append(f"{{{{< /{directive} >}}}}")
                    handled = True
                    break
            if handled:
                continue

            # Handle figures
            if this_line.strip().startswith(".. figure::"):
                shortcode, new_i = process_image_directive(inner_lines, current_idx)
                result_lines.append(shortcode)
                result_lines.append("")
                current_idx = new_i
                continue

            # Handle image directives
            if this_line.strip().startswith(".. image::"):
                shortcode, new_i = process_image_directive(inner_lines, current_idx)
                result_lines.append(shortcode)
                current_idx = new_i
                continue

            # Skip toctree
            if this_line.startswith(".. toctree::"):
                current_idx, _ = get_indented_block(
                    inner_lines, current_idx + 1, current_indent
                )
                continue

            # Handle grid tables
            if (
                this_line.strip()
                and (this_line.count("=") > 3 or this_line.count("-") > 3)
                and all(c in "=+-| " for c in this_line)
            ):
                # Check if this is likely a grid table by looking at surrounding lines
                is_grid_table = False

                # Check if there's a content line after this separator
                if (
                    current_idx + 1 < len(inner_lines)
                    and inner_lines[current_idx + 1].strip()
                ):
                    # If the next line has content and is followed by another separator, it's likely a table
                    if (
                        current_idx + 2 < len(inner_lines)
                        and inner_lines[current_idx + 2].strip()
                        and all(
                            c in "=+-| " for c in inner_lines[current_idx + 2].strip()
                        )
                    ):
                        is_grid_table = True
                    # Or if the next line has content with multiple spaces between words (column alignment)
                    elif "  " in inner_lines[current_idx + 1]:
                        is_grid_table = True

                if is_grid_table:
                    table_lines, new_i = process_grid_table(inner_lines, current_idx)
                    if table_lines:  # Only add if we successfully processed a table
                        result_lines.extend(table_lines)
                        current_idx = new_i
                        continue

            # Handle footnote definitions
            footnote_match = re.match(r"^\.\. \[([0-9#][^]]*)]", this_line.strip())
            if footnote_match:
                footnote_label = footnote_match.group(1)
                # Remove the # prefix if it exists (for auto-numbered or labeled footnotes)
                if footnote_label.startswith("#"):
                    footnote_label = footnote_label[1:]

                # Get the indentation of the current line
                current_indent = len(this_line) - len(this_line.lstrip())

                # Get the footnote text from the same line after the label
                rest_of_line = this_line.strip()[len(footnote_match.group(0)) :].strip()
                current_idx, new_lines = get_indented_block(
                    inner_lines, current_idx + 1, current_indent
                )

                # Store the footnote to add at the end of the document
                footnotes.append([f"[^{footnote_label}]: {rest_of_line}"] + new_lines)
                continue

            # Process the line for inline markup
            fixed_line = this_line
            match = bullet_regex.match(this_line)
            if match:
                indent, bullet = match.groups()
                indent_len = len(indent)
                if not indent_stack and indent_len > 0:
                    fixed_line = this_line.lstrip()
                elif indent_stack and indent_len > indent_stack[-1]:
                    indent_stack.append(indent_len)
                else:
                    while indent_stack and indent_len < indent_stack[-1]:
                        indent_stack.pop()
                    if not indent_stack:
                        indent_stack.append(indent_len)

            processed_line = process_inline_markup(fixed_line)
            processed_line = replace_substitutions(processed_line)

            # Fix image paths in markdown content
            if (
                "/components/" in filename
                and "![" in processed_line
                and "](/components/" in processed_line
            ):
                processed_line = processed_line.replace("](/components/", "](../")
            elif (
                "/components/" in filename
                and "![" in processed_line
                and "](images/" in processed_line
            ):
                processed_line = processed_line.replace("](images/", "](../images/")

            # Fix SVG image paths in markdown content
            if (
                "![" in processed_line
                and "](_build/_images/" in processed_line
                and ".svg)" in processed_line
            ):
                processed_line = processed_line.replace(
                    "](_build/_images/", "](/images/_build/_images/"
                )

            # Special handling for wireguard
            processed_line = re.sub(
                r"WireGuard®_",
                r"[WireGuard®](https://www.wireguard.org/)",
                processed_line,
            )
            # Add the processed line
            result_lines.append(processed_line)
            current_idx += 1
        return [x.rstrip() for x in result_lines]

    # Check for explicit title directive
    for i, line in enumerate(lines):
        if "This is a dummy file" in line:
            skip_build = True
        if line.startswith(".. title::"):
            explicit_title = line.replace(".. title::", "").strip()
            break

    # Find title (first line with underline of = or - characters)
    for i in range(len(lines) - 1):
        if re.match(r"^[=-]+$", lines[i + 1]) and lines[i]:
            title = lines[i]
            break

    # Use explicit title if available, otherwise use the heading title
    if explicit_title:
        title = explicit_title

    # Find all headings and collate the order

    i = 0
    while i < len(lines):
        line = lines[i]
        if (
            line
            and not line.startswith(".")
            and len(line.lstrip()) == len(line)
            and i + 1 < len(lines)
        ):
            underline = lines[i + 1]
            if re.match(r'^[-_=#+*~^".`]+', underline) and len(underline) >= len(line):
                underchar = underline[0]
                if all(x == underchar for x in underline):
                    if underchar not in heading_underlines:
                        heading_underlines.append(underchar)
        i += 1

    # Parse substitutions from original lines
    all_lines = lines[:]
    parse_substitutions(all_lines)
    # Remove substitution definitions from lines before further processing
    lines = remove_substitution_definitions(lines)
    lines = process_redirects(lines)
    lines = process_multiline_references(lines)
    lines = process_anchors_and_images(lines)

    md_lines = process_lines(lines)

    # Add footnotes at the end of the document if there are any
    if footnotes:
        for footnote in footnotes:
            md_lines.append("")  # Add a blank line before footnotes
            md_lines.extend(footnote)

    # Generate frontmatter
    frontmatter = ["---"]

    # Use description from SEO if available, otherwise use title
    description = seo.get("description", title)
    if not description:
        description = title

    # Avoid repeating "ESPHome" in the description if it's already in the title
    if title.startswith("ESPHome") and description.startswith("ESPHome"):
        description = description[len("ESPHome") :].strip()
        if description.startswith("-"):
            description = description[1:].strip()

    description = description.replace('"', '\\"')
    frontmatter.append(f'description: "{description}"')
    title = title.replace('"', '\\"')
    frontmatter.append(f'title: "{title}"')
    if skip_build:
        frontmatter.append("build: {render: never}")
    if seo:
        frontmatter.append("params:")
        frontmatter.append("  seo:")
        for k, v in seo.items():
            frontmatter.append(f"    {k}: {v}")
    frontmatter.append("---")
    # Make sure the file will end with a newline
    if md_lines[-1].strip() != "":
        md_lines.append("")

    return frontmatter + ["", ""] + md_lines


def process_inline_markup(line):
    """Process inline markup in a line of text."""
    processed_line = line

    # Code
    # processed_line = replace_substitutions(processed_line)
    # processed_line = rst_unicode_to_markdown(processed_line)
    processed_line = re.sub(r"``([^`]+)``", r"`\1`  ", processed_line)

    def footnote_ref_repl(match):
        ref = match.group(1).strip()
        if ref.startswith("#"):
            # For auto-numbered or labeled footnotes, remove the # prefix
            ref = ref[1:]
        return f"[^{ref}]"

    processed_line = re.sub(r"\[([0-9#a-zA-Z_]+)]_", footnote_ref_repl, processed_line)

    # Find and store all :ref: patterns
    def ref_repl(match):
        content = match.group(1)
        # Handle references with text and ID
        if "<" in content and ">" in content:
            text, ref_id = [x.strip() for x in content.split("<", 1)]
            ref_id = ref_id.rstrip(">")

            # Look up the document path for this anchor
            doc_path, anchor_text = anchor_map.get(ref_id, ("", ""))
            if not doc_path:
                print(f"Warning: Could not find document path for anchor '{ref_id}'")
            text = text or anchor_text
            ref_id = ref_id.lower()
            return f"[{text}](#{ref_id})"
        # Simple references
        # Look up the document path for this anchor
        doc_path, anchor_text = anchor_map.get(content, ("", ""))
        if not doc_path:
            print(f"Warning: Could not find document path for anchor '{content}'")
        anchor_text = anchor_text or content
        # If we can't find the document, just use the anchor
        return f"[{anchor_text}](#{content.lower()})"

    processed_line = re.sub(r":ref:`([^`]+)`", ref_repl, processed_line)

    def doc_repl(match):
        content = match.group(1).strip()

        # Handle document references with text and path
        if "<" in content and ">" in content:
            text, doc_path = [x.strip() for x in content.split("<", 1)]
            doc_path = doc_path.rstrip(">")
            # Use the docref shortcode with custom text
            return f'{{{{< docref "{doc_path}" "{text.strip()}" >}}}}'
            # Simple document references
        doc_path = fix_doc_path(content)
        return f'{{{{< docref "{doc_path}" >}}}}'

    processed_line = re.sub(r":doc:`([^`]+)`", doc_repl, processed_line)

    processed_line = re.sub(r":code:`([^`]+)`", r"`\1`", processed_line)

    processed_line = process_api(processed_line, "apiref")
    processed_line = process_api(processed_line, "apistruct")
    processed_line = process_api(processed_line, "apiclass")

    # Replace :ghuser:`username` or :ghuser:`text <username>` with the ghuser shortcode
    def ghuser_repl(match):
        content = match.group(1).strip()
        # Check for the form: text <username>
        m = re.match(r"([^<`]+)<([^>]+)>", content)
        if m:
            text = m.group(1).strip()
            username = m.group(2).strip()
            return f'{{{{< ghuser name="{username}" text="{text}" >}}}}'
        else:
            username = content
            return f'{{{{< ghuser name="{username}" >}}}}'

    processed_line = re.sub(r":ghuser:`([^`]+)`", ghuser_repl, processed_line)

    def esphome_repl(match):
        content = match.group(1).strip()
        number = content
        return f'{{{{< pr number="{number}" repo="esphome" >}}}}'

    processed_line = re.sub(r":esphomepr:`([^`]+)`", esphome_repl, processed_line)
    processed_line = re.sub(r":yamlpr:`([^`]+)`", esphome_repl, processed_line)

    def docs_repl(match):
        content = match.group(1).strip()
        number = content
        return f'{{{{< pr number="{number}" repo="esphome-docs" >}}}}'

    processed_line = re.sub(r":docspr:`([^`]+)`", docs_repl, processed_line)

    def lib_repl(match):
        content = match.group(1).strip()
        number = content
        return f'{{{{< pr number="{number}" repo="esphome-core" >}}}}'

    processed_line = re.sub(r":libpr:`([^`]+)`", lib_repl, processed_line)

    # External links
    processed_line = re.sub(
        r"`\s*([^`]*[^` ]+)\s*<([^`]+)>`_+", r"[\1](\2)", processed_line
    )
    processed_line = re.sub(r"^\.\. _([^:]+):\s*(http.*)$", r"", processed_line)

    if ":ghedit:" in processed_line:
        processed_line = ""

    return processed_line


def process_api(processed_line, shortcode):
    def api_repl(match):
        content = match.group(1).strip()

        # Handle API references with text and path
        if "<" in content and ">" in content:
            text, api_path = content.split("<", 1)
            api_path = api_path.rstrip(">")
            # Use the apiref shortcode with custom text
            return f'{{{{< {shortcode} "{text.strip()}" "{api_path}" >}}}}'
        return f'{{{{< {shortcode} "{content}" "{content}" >}}}}'

    return re.sub(rf":{shortcode}:`([^`]+)`", api_repl, processed_line)


def process_multiline_references(lines):
    """Process references that might be split across multiple lines."""
    processed_lines = []
    i = 0

    while i < len(lines):
        current_line = lines[i]

        # Check if this line might contain the start of a split reference
        if (
            current_line.count("`") % 2 == 1
            and ">" not in current_line
            and i + 1 < len(lines)
        ):
            # Look ahead to see if the next line completes the reference
            next_line = lines[i + 1]
            if ">" in next_line and ("`_" in next_line or ">" in next_line):
                # This is a split reference, combine the lines
                combined_line = current_line + next_line
                # Process the combined line
                processed_line = process_inline_markup(combined_line)
                processed_lines.append(processed_line)
                i += 2  # Skip the next line since we've processed it
                continue

        # Normal line processing
        processed_lines.append(process_inline_markup(current_line))
        i += 1

    return processed_lines


def parse_substitutions(lines):
    global substitutions
    subs = {}
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(r"^\s*\.\.\s+\|([^|]+)\|\s+(.*)", line)
        if m:
            subname = m.group(1).strip()
            html_lines = [rst_unicode_to_markdown(m.group(2).strip())]
            i += 1
            # Collect indented HTML lines
            while i < len(lines) and (
                lines[i].strip() == "" or lines[i].startswith("   ")
            ):
                if lines[i].strip() != "":
                    html_lines.append(lines[i].lstrip())
                i += 1
            subs[subname] = "\n".join(html_lines)
        else:
            i += 1
    substitutions = subs


def replace_substitutions(line):
    def repl(m):
        name = m.group(1)
        return substitutions.get(name, m.group(0))

    return re.sub(r"\|([^|]+)\|", repl, line)


def remove_substitution_definitions(lines):
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(r"^\s*\.\.\s+\|([^|]+)\|\s+(.*)", line)
        if m:
            i += 1
            # Skip indented HTML lines
            while i < len(lines) and (
                lines[i].strip() == "" or lines[i].startswith("   ")
            ):
                i += 1
            continue  # skip this definition block
        out.append(line)
        i += 1
    return out


def process_redirects(lines):
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if re.match(r"^\s*\.\.\s+redirect::\s*$", line):
            url = None
            i += 1
            while i < len(lines) and (
                lines[i].strip() == "" or lines[i].startswith("   ")
            ):
                m = re.match(r"^\s*:url:\s*(\S+)", lines[i].strip())
                if m:
                    url = m.group(1)
                i += 1
            if url:
                out.append(f'{{{{< redirect url="{url}" >}}}}')
            continue
        out.append(line)
        i += 1
    return out


def process_anchors_and_images(lines):
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        indent = re.match(r"^(\s*)", line).group(1)
        # Detect RST anchor (.. _anchor:)
        anchor_match = re.match(r"^\s*\.\.\s*_([a-zA-Z0-9_-]+):\s*$", line)
        if anchor_match:
            anchor = anchor_match.group(1)
            out.append(f'{indent}{{{{< anchor "{anchor.lower()}" >}}}}')
            i += 1
            continue
        # Detect indented .. image:: (even inside lists/admonitions)
        image_match = re.match(r"^\s*\.\.\s*image::\s*(\S+)\s*$", line)
        if image_match:
            image_path = image_match.group(1)
            if image_path.endswith(".avif"):
                image_path = image_path.replace(".avif", ".webp")
            # Collect options (indented lines)
            options = {}
            j = i + 1
            while j < len(lines) and (lines[j].strip() and lines[j].startswith("   ")):
                opt_line = lines[j].strip()
                m = re.match(r":([a-zA-Z0-9_-]+):\s*(.*)", opt_line)
                if m:
                    options[m.group(1)] = m.group(2)
                j += 1
            alt = options.get("alt", "")
            width = options.get("width")
            height = options.get("height")
            # Build img shortcode
            shortcode = f'{{{{< img src="{image_path}" alt="{alt}" '
            if width:
                shortcode += f' width="{width}"'
            if height:
                shortcode += f' height="{height}"'
            shortcode += " >}}"
            out.append(f"{indent}{shortcode}")
            i = j
            continue
        out.append(line)
        i += 1
    return out


def convert_image_directive_in_text(text, indent=""):
    # Handles .. image:: and .. figure:: path [options] in a text block (single or multiline)
    lines = text.splitlines()
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        img_match = re.match(r"^\s*\.\.\s*(image|figure)::\s*(\S+)\s*$", line)
        if img_match:
            image_path = img_match.group(2)
            if image_path.endswith(".avif"):
                image_path = image_path.replace(".avif", ".webp")
            is_figure = img_match.group(1) == "figure"
            options = {}
            caption_lines = []
            j = i + 1
            # Collect options and caption (for figure::)
            while j < len(lines) and (lines[j].strip() and lines[j].startswith("   ")):
                opt_line = lines[j].strip()
                m = re.match(r":([a-zA-Z0-9_-]+):\s*(.*)", opt_line)
                if m:
                    options[m.group(1)] = m.group(2)
                elif is_figure:
                    caption_lines.append(opt_line)
                j += 1
            alt = options.get("alt", "")
            width = options.get("width")
            height = options.get("height")
            caption = ""
            if is_figure and caption_lines:
                # Join and process caption lines as inline markup
                caption = " ".join(caption_lines)
            shortcode = f'{{{{< img src="{image_path}" alt="{alt}" '
            if caption:
                shortcode += f'caption="{caption}" '
            if width:
                shortcode += f'width="{width}" '
            if height:
                shortcode += f'height="{height}" '
            shortcode += ">}}"
            out.append(f"{indent}{shortcode}")
            i = j
            continue
        out.append(line)
        i += 1
    return "\n".join(out)


def get_indent(line):
    return len(line) - len(line.lstrip())


def process_list_table(lines, start_idx):
    """Process a list-table directive and convert it to a Markdown table."""
    # Extract table title and options
    title = ""
    header_rows = 0
    align = ""

    current_line = lines[start_idx].strip()
    if current_line.startswith(".. list-table::") or current_line.startswith(
        "..  list-table::"
    ):
        title = (
            current_line.replace(".. list-table::", "")
            .replace("..  list-table::", "")
            .strip()
        )

    # Process table options
    idx = start_idx + 1
    while idx < len(lines) and lines[idx].strip().startswith(":"):
        option_line = lines[idx].strip()
        if option_line.startswith(":header-rows:"):
            try:
                header_rows = int(option_line.split(":", 2)[2].strip())
            except (ValueError, IndexError):
                pass
        elif option_line.startswith(":width:"):
            pass  # width = option_line.split(':', 2)[2].strip()
        elif option_line.startswith(":widths:"):
            pass
        elif option_line.startswith(":align:"):
            align = option_line.split(":", 2)[2].strip()
        elif option_line.startswith(":class:"):
            # We don't use class in Markdown, but we'll parse it anyway
            pass
        idx += 1

    # Skip any blank lines
    while idx < len(lines) and not lines[idx].strip():
        idx += 1

    # Process table rows
    table_data = []
    current_row = []
    anchor = ""

    while idx < len(lines):
        line = lines[idx].strip()

        # End of table when we hit a non-indented line after a blank line
        if not line:
            if idx + 1 < len(lines) and not lines[idx + 1].startswith("    "):
                break
            idx += 1
            continue

        if (
            line.strip().startswith(".. _")
            and line.endswith(":")
            or line.strip().startswith("{{")
        ):
            anchor = line
            idx += 1
            continue
        # New row starts with *
        if line.startswith("*"):
            if current_row:
                table_data.append(current_row)
            current_row = []

            # Extract the first cell value
            cell_value = line[1:].strip()
            if cell_value.startswith("-"):
                cell_value = anchor.strip() + cell_value[1:].strip()
                anchor = ""
                current_row.append(cell_value)

            idx += 1
            continue

        # Cell values start with -
        if line.startswith("-"):
            cell_value = [line[1:].strip()]
            this_indent = get_indent(lines[idx])
            idx += 1
            while idx < len(lines):
                if lines[idx].strip().startswith("{{"):
                    anchor = lines[idx]
                    idx += 1
                    continue
                if lines[idx].strip() and get_indent(lines[idx]) > this_indent:
                    cell_value.append(lines[idx])
                    idx += 1
                    continue
                if (
                    not lines[idx].strip()
                    and idx + 1 < len(lines)
                    and get_indent(lines[idx + 1]) > this_indent
                ):
                    cell_value.append(lines[idx])
                    idx += 1
                    continue
                break
            cell_text = process_cell_value(cell_value)
            current_row.append(cell_text)
            continue
        # If we get here, it's either the end of the table or something we don't understand
        if not lines[idx].startswith("    "):
            break

        idx += 1

    # Add the last row if it has content
    if current_row:
        table_data.append(current_row)

    # Generate Markdown table
    md_table = []

    # Add title if present and not empty
    if title and title.strip():
        # Remove any leading colon from the title
        if title.startswith(":"):
            title = title[1:].strip()
        md_table.append(f"### {title}")
        md_table.append("")

    # Ensure all rows have the same number of columns
    if table_data:
        max_cols = max(len(row) for row in table_data)
        for row in table_data:
            while len(row) < max_cols:
                row.append("")

        # Create the table header
        if header_rows > 0:
            header_row = table_data[0]
            md_table.append(
                "| "
                + " | ".join(
                    convert_image_directive_in_text(cell) for cell in header_row
                )
                + " |"
            )

            # Add alignment to the separator row if specified
            if align == "center":
                md_table.append("| " + " | ".join([":---:"] * len(header_row)) + " |")
            elif align == "right":
                md_table.append("| " + " | ".join(["---:"] * len(header_row)) + " |")
            elif align == "left":
                md_table.append("| " + " | ".join([":---"] * len(header_row)) + " |")
            else:
                md_table.append("| " + " | ".join(["---"] * len(header_row)) + " |")

            # Add data rows
            for row in table_data[header_rows:]:
                md_table.append(
                    "| "
                    + " | ".join(convert_image_directive_in_text(cell) for cell in row)
                    + " |"
                )
        else:
            # No header, just data rows
            for row in table_data:
                md_table.append(
                    "| "
                    + " | ".join(convert_image_directive_in_text(cell) for cell in row)
                    + " |"
                )

    # Add a blank line after the table
    md_table.append("")

    return md_table, idx


def process_cell_value(cell_value):
    if isinstance(cell_value, str):
        cell_value = cell_value.split("\n")
    cell_text = ""
    i = 0
    while i < len(cell_value):
        if "figure::" in cell_value[i] or "image::" in cell_value[i]:
            shortcode, i = process_image_directive(cell_value, i)
            cell_text = " ".join([cell_text, shortcode])
        else:
            cell_text = " ".join([cell_text, cell_value[i].strip()])
        i += 1
    return process_inline_markup(cell_text)


def process_csv_table(lines, start_idx):
    """Process a csv-table directive and convert it to a Markdown table."""
    # Extract table title and options
    title = ""
    header_rows = None
    align = ""
    delimiter = ","

    current_line = lines[start_idx].strip()
    if current_line.startswith(".. csv-table::"):
        title = current_line.replace(".. csv-table::", "").strip()

    # Process table options
    idx = start_idx + 1
    while idx < len(lines) and lines[idx].strip().startswith(":"):
        option_line = lines[idx].strip()
        if option_line.startswith(":header:"):
            header_rows = normalize_csv_lines([option_line.split(":", 2)[2].strip()])[0]
        elif option_line.startswith(":width:"):
            pass  # width = option_line.split(':', 2)[2].strip()
        elif option_line.startswith(":align:"):
            align = option_line.split(":", 2)[2].strip()
        elif option_line.startswith(":delim:"):
            delimiter = option_line.split(":", 2)[2].strip()
            # Handle special delimiter cases
            if delimiter == "tab":
                delimiter = "\t"
            elif delimiter == "space":
                delimiter = " "
        elif option_line.startswith(":file:"):
            # Handle CSV file inclusion
            csv_file = option_line.split(":", 2)[2].strip()
            # This would need to be implemented to read from the file
            # For now, we'll just print a warning
            print(f"Warning: CSV file inclusion not yet supported: {csv_file}")
        idx += 1

    # Skip any blank lines
    while idx < len(lines) and not lines[idx].strip():
        idx += 1

    # Process table rows
    table_data = []

    while idx < len(lines):
        line = lines[idx].strip()

        # End of table when we hit a non-indented line after a blank line
        if not line:
            if idx + 1 < len(lines) and not lines[idx + 1].startswith("    "):
                break
            idx += 1
            continue

        # End of table when we hit a line that doesn't start with whitespace
        if not lines[idx].startswith("    "):
            break

        # Process CSV line
        # Remove leading whitespace but keep the rest of the line intact
        table_data.append(line.strip())
        idx += 1

    table_data = normalize_csv_lines(table_data, delimiter)
    # Generate Markdown table
    md_table = []

    # Add title if present and not empty
    if title and title.strip():
        # Remove any leading colon from the title
        if title.startswith(":"):
            title = title[1:].strip()
        md_table.append(f"### {title}")
        md_table.append("")

    # Ensure all rows have the same number of columns
    if table_data:
        max_cols = max(len(row) for row in table_data)
        for row in table_data:
            while len(row) < max_cols:
                row.append("")

        # Create the table header and separator
        if header_rows:
            # Add header row
            md_table.append(
                "| "
                + " | ".join(
                    convert_image_directive_in_text(cell) for cell in header_rows
                )
                + " |"
            )

            # Add alignment to the separator row if specified
            if align == "center":
                md_table.append("| " + " | ".join([":---:"] * len(header_rows)) + " |")
            elif align == "right":
                md_table.append("| " + " | ".join(["---:"] * len(header_rows)) + " |")
            elif align == "left":
                md_table.append("| " + " | ".join([":---"] * len(header_rows)) + " |")
            else:
                md_table.append("| " + " | ".join(["---"] * len(header_rows)) + " |")

            # Add data rows
            for row in table_data:
                md_table.append(
                    "| "
                    + " | ".join(convert_image_directive_in_text(cell) for cell in row)
                    + " |"
                )
        else:
            # No header specified, but we still need to add a separator after the first row
            if table_data:
                # Add first row
                first_row = table_data[0]
                md_table.append(
                    "| "
                    + " | ".join(
                        convert_image_directive_in_text(cell) for cell in first_row
                    )
                    + " |"
                )

                # Add separator row
                if align == "center":
                    md_table.append(
                        "| " + " | ".join([":---:"] * len(first_row)) + " |"
                    )
                elif align == "right":
                    md_table.append("| " + " | ".join(["---:"] * len(first_row)) + " |")
                elif align == "left":
                    md_table.append("| " + " | ".join([":---"] * len(first_row)) + " |")
                else:
                    md_table.append("| " + " | ".join(["---"] * len(first_row)) + " |")

                # Add remaining data rows
                for row in table_data[1:]:
                    md_table.append(
                        "| "
                        + " | ".join(
                            convert_image_directive_in_text(cell) for cell in row
                        )
                        + " |"
                    )

    # Add a blank line after the table
    md_table.append("")

    return md_table, idx


def process_grid_table(lines, start_idx):
    """Process a grid table directive and convert it to a Markdown table.

    Grid tables in RST can be formatted in two ways:
    1. With rows separated by lines of "=" or "-" characters and columns separated by "|" characters
    2. With rows separated by lines of "=" or "-" characters and columns aligned by whitespace

    Args:
        lines: List of lines in the file
        start_idx: Index of the first line of the grid table

    Returns:
        Tuple of (markdown_table_lines, end_idx)
    """
    i = start_idx
    table_lines = []

    # Collect all lines of the table
    while i < len(lines):
        line = lines[i]
        # If we hit an empty line after the table has started, we're done
        if not line.strip() and table_lines:
            break

        # Add any line that's part of the table structure
        if line.strip():
            table_lines.append(line)
        else:
            # Not part of the table
            break

        i += 1

    # Process the table
    if not table_lines:
        return [], start_idx

    # Determine if this is a table with | separators or whitespace alignment
    has_pipe_separators = "+" in table_lines[0]

    if has_pipe_separators:
        return process_pipe_separated_table(table_lines), i
    else:
        return process_whitespace_aligned_table(table_lines, i)


def process_pipe_separated_table(lines):
    # Find border lines and split table into row-blocks
    rows = []
    current_block = []
    for line in lines:
        if line.startswith("+") and "=" in line:
            # End of header row
            if current_block:
                rows.append(current_block)
                current_block = []
            rows.append(["HEADER-SEPARATOR"])
        elif line.startswith("+") and "-" in line:
            # End of normal row
            if current_block:
                rows.append(current_block)
                current_block = []
        elif "|" in line:
            cells = [c.strip() for c in line.split("|")[1:-1]]
            current_block.append(cells)

    # Merge multi-line cells inside each block
    merged_rows = []
    for block in rows:
        if block == ["HEADER-SEPARATOR"]:
            merged_rows.append(block)
            continue
        merged = []
        for col_parts in zip_longest(*block, fillvalue=""):
            merged.append(" ".join(part for part in col_parts if part))
        merged_rows.append(merged)

    # Now build Markdown
    md_lines = []
    for row in merged_rows:
        if row == ["HEADER-SEPARATOR"]:
            # Add Markdown separator for header
            md_lines.append("|" + "|".join("---" for _ in merged_rows[0]) + "|")
        else:
            md_lines.append("| " + " | ".join(row) + " |")

    return md_lines


def process_whitespace_aligned_table(table_lines, end_idx):
    """Process a grid table with whitespace alignment."""
    # Find separator rows (rows with only =, -, + and spaces)
    separator_rows = []
    for idx, line in enumerate(table_lines):
        if all(c in "=+-| " for c in line):
            separator_rows.append(idx)

    # Identify the header rows (usually the first and last rows with = characters)
    header_rows = []
    for idx, line in enumerate(table_lines):
        if "=" in line and all(c in "=+-| " for c in line):
            header_rows.append(idx)

    # Find column boundaries by analyzing the content rows
    # We'll look at the first content row after the first separator
    content_rows = [idx for idx in range(len(table_lines)) if idx not in separator_rows]

    # If we have no content rows, return empty
    if not content_rows:
        return [], end_idx

    column_positions = []
    first_header_row = min(header_rows)
    first_header = table_lines[first_header_row]

    # Find column positions by looking for groups of non-space characters
    in_column = False
    for i, char in enumerate(first_header):
        if not in_column and char != " ":
            # Start of a column
            in_column = True
            column_positions.append(i)
        elif in_column and char == " ":
            # End of a column (followed by at least one more space or end of line)
            in_column = False

    # Add an end position if needed
    max_line_length = max(len(line) for line in table_lines)
    if column_positions and column_positions[-1] < max_line_length:
        column_positions.append(max_line_length)

    column_count = len(column_positions)

    # Process each row
    header_added = False
    rows = []
    for line_idx, line in enumerate(table_lines):
        # Skip separator rows for Markdown output
        if line_idx in separator_rows:
            # If this is the separator after the header row, add a Markdown header separator
            if not header_added and line_idx > 0 and line_idx - 1 not in separator_rows:
                header_added = True
                header_cells = ["---" for _ in range(column_count - 1)]
                rows.append(header_cells)
            continue

        # Extract cells from the row
        cells = []
        for i in range(column_count):
            start_pos = column_positions[i]
            # End position is either the next column start or the end of the line
            end_pos = column_positions[i + 1] if i + 1 < column_count else len(line)

            # Make sure we don't go out of bounds
            if start_pos < len(line):
                cell_content = line[start_pos : min(end_pos, len(line))].strip()
                cell_content = process_cell_value(replace_substitutions(cell_content))
                cells.append(cell_content)

        # Add the row to Markdown output
        if len(cells) < column_count - 1:
            cells += ["" for _ in range(column_count - 1 - len(cells))]
        if header_added and not cells[0].strip():
            rows[-1] = [p + " " + n for p, n in zip(rows[-1], cells)]
        else:
            rows.append(cells)
    markdown_rows = ["| " + " | ".join(row) + " |" for row in rows]
    return markdown_rows, end_idx


def process_raw_html_block(lines, i):
    """Process raw HTML button patterns and convert them to button shortcode."""
    button_lines = []
    raw_html_indent = len(lines[i]) - len(lines[i].lstrip())

    i, raw_html_content = get_indented_block(lines, i + 1, raw_html_indent)
    html_content = [x.strip() for x in raw_html_content]
    html = " ".join(html_content)

    # --- API KEY SHORTCODE REPLACEMENT (for api.rst) ---
    if '<input type="text"' in html and 'id="api-key"' in html and "<script" in html:
        return ["{{< api-key-input >}}"], i
    # --- END API KEY SHORTCODE REPLACEMENT ---

    # Check if it's a button pattern
    href_match = re.search(r'<a\s+href="([^"]+)"[^>]*>', html)
    img_match = re.search(r'<img\s+src="([^"]+)"[^>]*alt="([^"]*)"[^>]*/?>', html)

    if href_match and img_match:
        href = href_match.group(1)
        img = img_match.group(1)
        alt = img_match.group(2)

        # Create button shortcode
        button_lines.append(f"[![{alt}]({img})]({href})")
        return button_lines, i

    file_match = None
    class_match = None
    for line in raw_html_content:
        if not file_match:
            file_match = re.search(r":file: (.+)", line.strip())
        if not class_match:
            class_match = re.search(r":class: (.+)", line.strip())

    if file_match:
        href = file_match.group(1)
        href = href.replace("../", "", 1)
        class_ = ""
        if class_match:
            classes = class_match.group(1).replace(",", " ").strip()
            class_ = 'class="' + classes + '"'
        button_lines.append(f'{{{{< html_file file="{href}" {class_} >}}}}')
        button_lines.append("")
        return button_lines, i

    # If it's not a button pattern, just keep the raw HTML
    button_lines.append("\n".join(raw_html_content))
    return button_lines, i


def process_image_directive(lines, i):
    """Process an image or figure directive and convert it to a Hugo shortcode."""
    line = lines[i]

    is_figure = False
    if "figure::" in line:
        is_figure = True
        image_path = line.replace("(\\.\\. )?figure::", "").strip()
    else:
        image_path = line.replace("(\\.\\. )?image::", "").strip()

    # Extract the image filename
    if image_path.endswith(".avif"):
        image_path = image_path.replace(".avif", ".webp")
    image_filename = os.path.basename(image_path)

    # Skip options
    i += 1

    # Process options
    alt_text = "Image"
    caption = ""
    width = ""
    height = ""
    align = ""

    while i < len(lines) and (
        not lines[i].strip()
        or lines[i].startswith("  ")
        and lines[i].strip().startswith(":")
    ):
        option_line = lines[i].strip()
        if option_line.startswith(":alt:"):
            alt_text = option_line.replace(":alt:", "").strip()
        elif option_line.startswith(":width:"):
            width = option_line.split(":", 2)[2].strip()
        elif option_line.startswith(":height:"):
            height = option_line.split(":", 2)[2].strip()
        elif option_line.startswith(":align:"):
            align = option_line.split(":", 2)[2].strip()
        i += 1

    # Get caption if present (for figures)
    caption_lines = []
    while i < len(lines) and lines[i].startswith("  ") and is_figure:
        caption_lines.append(lines[i].strip())
        i += 1

    # Skip any blank lines after the caption
    while i < len(lines) and not lines[i].strip():
        i += 1

    # Process caption for inline markup if present
    if caption_lines:
        # Join caption lines into a single string
        caption_text = " ".join(caption_lines)
        # Process the caption for inline markup (references, formatting, etc.)
        caption = process_inline_markup(caption_text)

    # Escape quotes in alt text and caption
    if alt_text:
        alt_text = alt_text.replace('"', '\\"')
    if caption:
        caption = caption.replace('"', '\\"')

    # Create the shortcode
    shortcode = f'{{{{< img src="{image_filename}" alt="{alt_text}" '
    if caption:
        shortcode += f'caption="{caption}" '
    if width:
        shortcode += f'width="{width}" '
    if height:
        shortcode += f'height="{height}" '
    if align:
        shortcode += f'class="align-{align}" '
    shortcode += ">}}"

    return shortcode, i


def process_includes(lines, current_dir):
    """Process include directives in RST files."""
    processed_lines = []
    i = 0

    while i < len(lines):
        line = lines[i]

        # Check for include directive
        include_match = re.match(r"^(\s*)\.\.[\s]+include::[\s]+(.+\.rst)$", line)
        if include_match:
            indent = include_match.group(1)
            include_file = include_match.group(2).strip()

            # Construct the full path to the included file
            include_path = os.path.join(current_dir, include_file)

            if os.path.exists(include_path):
                # Read the included file
                with open(include_path, "r", encoding="utf-8") as f:
                    include_content = f.read()

                # Process the included content
                include_lines = include_content.split("\n")

                # Add indentation to all lines from the included file
                for include_line in include_lines:
                    if include_line.strip():
                        processed_lines.append(f"{indent}{include_line}")
                    else:
                        processed_lines.append("")
            else:
                # If the file doesn't exist, just keep the include directive as a comment
                processed_lines.append(
                    f"{indent}<!-- Include not found: {include_file} -->"
                )
        else:
            processed_lines.append(line)

        i += 1

    return processed_lines


def fix_doc_path(path):
    """Fix document paths to match Hugo's content structure."""
    # Remove .rst extension if present
    path = path.lower()
    if path.endswith(".rst"):
        path = path[:-4]
    if path.endswith("/index"):
        path = path[:-6]

    # Handle special cases for components
    #    if path.startswith('components/'):
    #        path = path[11:]  # Remove 'components/' prefix
    #    elif '/' in path and not path.startswith('/'):
    #        # For paths like 'switch/gpio', we need to make them '/components/switch/gpio'
    #        path = f"/components/{path}"

    # Don't add trailing slash for file references
    # Check if it's likely a file reference (contains no hash and has a name after the last slash)
    if not path.startswith("#") and "/" in path:
        last_part = path.split("/")[-1]
        # If the last part looks like a filename (not empty and doesn't end with a slash)
        if last_part and not path.endswith("/"):
            # Don't add a trailing slash
            return path

    # Add trailing slash for section references (if not to a specific anchor)
    if not path.startswith("#") and not path.endswith("/") and "#" not in path:
        path = f"{path}/"

    return path


def scan_image_references(input_dir):
    """Scan all RST files for image references and track their usage."""
    print("Scanning for image references...")

    # Regular expressions to match different types of image references
    image_patterns = [
        r".. figure:: ([^\s]+)",  # Figure directive
        r".. image:: ([^\s]+)",  # Image directive
        r"image:: ([^\s]+)",  # Image reference
        r":image: ([^\s]+)",  # Seo Image reference
        r'src="([^"]+\.(avif|png|jpg|jpeg|gif|svg))"',  # HTML img tag
        r"!\[(.*?)\]\(([^)]+\.(avif|png|jpg|jpeg|gif|svg))\)",  # Markdown image syntax
    ]

    # Initialize image tracking dictionaries
    result = {}

    for root, _, files in os.walk(input_dir):
        for file in files:
            if file.endswith(".rst"):
                src_file = os.path.join(root, file)

                with open(src_file, "r", encoding="utf-8") as f:
                    content = f.read()
                    lines = content.splitlines()

                # Find all image references using regex patterns
                for pattern in image_patterns:
                    for match in re.finditer(pattern, content):
                        image_path = match.group(1).strip()

                        # Skip alignment options and other non-image paths
                        if image_path in ["center", "left", "right"]:
                            continue

                        # Skip URLs
                        if image_path.startswith(("http://", "https://")):
                            continue

                        # Normalize path
                        if image_path.startswith("/"):
                            # Absolute path within docs
                            abs_image_path = os.path.join(
                                input_dir, image_path.lstrip("/")
                            )
                        else:
                            # Relative path
                            abs_image_path = os.path.join(
                                os.path.dirname(src_file), image_path
                            )
                            if not os.path.exists(abs_image_path):
                                abs_image_path = os.path.join(
                                    input_dir, "images/" + image_path.lstrip("/")
                                )

                        # Only count if the image file exists
                        if os.path.exists(abs_image_path):
                            image_filename = os.path.basename(image_path)
                            entry = result.setdefault(
                                image_filename,
                                ImageInfo(image_filename, abs_image_path, src_file),
                            )
                            entry.increment()
                            # print(f"Found image: {image_filename} in {rel_path}")

                # Find images in imgtable directives
                i = 0
                while i < len(lines):
                    line = lines[i].strip()

                    # Check for imgtable directive
                    if line == ".. imgtable::":
                        i += 1
                        # Skip empty lines
                        while i < len(lines) and not lines[i].strip():
                            i += 1

                        # Process each entry in the imgtable
                        while i < len(lines):
                            current_line = lines[i].strip()

                            # If we hit an empty line or a non-indented line, we're done with this imgtable
                            if not lines[i].startswith("    ") and current_line:
                                break

                            # Skip empty lines within the imgtable
                            if not current_line:
                                i += 1
                                continue

                            # Process the entry - format is typically: Title, Link, Image, [Description]
                            parts = [part.strip() for part in current_line.split(",")]
                            if (
                                len(parts) >= 3
                            ):  # We need at least 3 parts (title, link, image)
                                image_path = parts[2]

                                # Skip URLs
                                if image_path.startswith(("http://", "https://")):
                                    i += 1
                                    continue

                                # Normalize path
                                if image_path.startswith("/"):
                                    # Absolute path within docs
                                    abs_image_path = os.path.join(
                                        input_dir, image_path.lstrip("/")
                                    )
                                else:
                                    # Relative path
                                    abs_image_path = os.path.join(
                                        os.path.dirname(src_file), image_path
                                    )
                                    if not os.path.exists(abs_image_path):
                                        abs_image_path = os.path.join(
                                            input_dir, "images", image_path
                                        )

                                # Only count if the image file exists
                                if os.path.exists(abs_image_path):
                                    image_filename = os.path.basename(image_path)
                                    entry = result.setdefault(
                                        image_filename,
                                        ImageInfo(
                                            image_filename, abs_image_path, src_file
                                        ),
                                    )
                                    entry.increment()
                                    # print(f"Found image in imgtable: {image_filename} in {rel_path}")
                                else:
                                    print(
                                        f"Image not found: {image_path} in {abs_image_path}"
                                    )

                            i += 1
                    else:
                        i += 1

    # Print statistics
    print(f"Found {len(result)} unique images")
    multiple = [image for image in result.values() if image.count > 1]
    print(f"Images used more than once: {len(multiple)}")

    return result


# Special handling for some files


def process_actions_file(lines):
    start_actions = lines.index("## All Actions")
    end_actions = lines.index('{{< anchor "tips-and-tricks" >}}')

    if start_actions and end_actions:
        lines = (
            lines[:start_actions]
            + [
                "## All Actions",
                '{{< render-automations "actions" >}}',
                '{{< anchor "config-condition" >}}',
                "## All Conditions",
                '{{< render-automations "conditions" >}}',
            ]
            + lines[end_actions:]
        )
    return lines


def process_file(src_file, output_dir, input_dir):
    output_dir = os.path.join(output_dir, "content")
    """Process a single RST file and convert it to Markdown."""
    # print(rf"Processing file: {src_file}", end="", flush=True)
    try:
        # Read the RST file
        rel_path, rst_content = get_rst_content(input_dir, src_file)

        # Convert RST to Markdown
        md_content = convert_rst_to_md(rst_content, rel_path)

        # Determine output path
        if os.path.basename(src_file) == "index.rst":
            # Convert index.rst to _index.md for Hugo
            output_path = os.path.join(
                output_dir, os.path.dirname(rel_path), "_index.md"
            )
        else:
            output_path = os.path.join(
                output_dir, os.path.splitext(rel_path)[0] + ".md"
            )

        if output_path.endswith("automations/actions.md"):
            md_content = process_actions_file(md_content)

        # Create output directory if it doesn't exist
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        # Write the Markdown file
        # If the file already exists we don't need to tell git to move it
        if not os.path.exists(output_path):
            repo = Repo(".")
            repo.git.mv(src_file, output_path, "-f")
        with open(output_path, "w", encoding="utf-8") as f:
            f.write("\n".join(md_content))

        # print(f"  Converted {src_file} -> {output_path}\033G", end="", flush=True)
        # print(f"Output file size: {len(md_content)} bytes")

        return output_path
    except Exception as e:
        print(f"Error converting {src_file}: {str(e)}")
        import traceback

        traceback.print_exc()
        return None


def get_rst_content(input_dir, src_file):
    with open(src_file, "r", encoding="utf-8") as f:
        rst_content = f.read()
    # print(f"File size: {len(rst_content)} bytes")
    # Get the relative path of the file
    rel_path = os.path.relpath(src_file, input_dir)
    # Process includes before conversion
    current_dir = os.path.dirname(src_file)
    rst_lines = rst_content.split("\n")
    rst_lines = process_includes(rst_lines, current_dir)
    return rel_path, rst_lines


def process_directory(input_dir, output_dir):
    """Process all RST files in a directory and its subdirectories."""
    success_count = 0
    total_count = 0

    rstfiles = []
    for root, _, files in os.walk(input_dir):
        for file in files:
            if file.endswith(".rst"):
                fullpath = os.path.join(root, file)
                rstfiles.append(fullpath)
                included_files.update(set(find_included_files(fullpath)))

    for fullpath in rstfiles:
        root = os.path.dirname(fullpath)
        if (
            os.path.basename(root) == input_dir
            and os.path.basename(fullpath) == "index.rst"
        ):
            print("Skipping top-level index.rst file:", fullpath)
            continue
        if fullpath in included_files:
            print("Skipping included file:", fullpath)
            continue
        elif fullpath.endswith(".rst"):
            included_files.update(set(find_included_files(fullpath)))
            total_count += 1
            if process_file(fullpath, output_dir, input_dir):
                success_count += 1

    print(
        f"Conversion complete. {success_count}/{total_count} files successfully converted to {output_dir}"
    )


def should_copy_file(source_path, target_path):
    """
    Determine if a file should be copied based on existence and modification time.
    Returns True if the target doesn't exist or if the source is newer than the target.
    """
    if not os.path.exists(target_path):
        return True

    # Check if source is newer than target
    source_mtime = os.path.getmtime(source_path)
    target_mtime = os.path.getmtime(target_path)

    return source_mtime > target_mtime


def copy_images_to_output(output_dir, input_dir, replace=False):
    """Copy images to the appropriate locations based on usage."""
    print("Copying images to output directories...")

    # Create global images directory
    global_images_dir = os.path.join(output_dir, "static", "images")
    os.makedirs(global_images_dir, exist_ok=True)

    # Copy images based on usage
    for image in image_map.values():
        source_path = image.path
        target_name = image.name
        if image.name.endswith(".avif"):
            continue

        if image.count > 1:
            # Used more than once - copy to global images folder
            target_path = os.path.join(global_images_dir, target_name)
        else:
            # Used only once - copy to component-level images folder
            # Find the RST file that references this image
            rel_path = os.path.relpath(image.source, input_dir)
            component_dir = os.path.dirname(rel_path)

            # Create component-level images directory in content
            component_content_dir = os.path.join(output_dir, "content", component_dir)
            component_images_dir = os.path.join(component_content_dir, "images")
            os.makedirs(component_images_dir, exist_ok=True)

            target_path = os.path.join(component_images_dir, target_name)

        if replace:
            repo = Repo(".")
            repo.git.mv(image.path, target_path, "-f")
        else:
            shutil.copy2(source_path, target_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Convert Sphinx RST files to Hugo Markdown format"
    )
    parser.add_argument("input_dir", help="Input directory containing RST files")
    parser.add_argument("output_dir", help="Output directory for Markdown files")
    parser.add_argument(
        "--single", help="Process a single file (relative to input_dir)"
    )
    parser.add_argument(
        "--no-images", action="store_true", help="Skip image processing"
    )
    args = parser.parse_args()

    # Ensure output directory exists
    os.makedirs(args.output_dir, exist_ok=True)
    os.makedirs(os.path.join(args.output_dir, "content"), exist_ok=True)
    os.makedirs(os.path.join(args.output_dir, "static"), exist_ok=True)

    # Build the anchor map first
    get_all_included_files(args.input_dir)
    build_anchor_map(args.input_dir)

    # Scan for image references
    image_map = scan_image_references(args.input_dir)

    if args.single:
        # Process a single file
        rst_file = os.path.join(args.input_dir, args.single)
        if os.path.exists(rst_file):
            process_file(rst_file, args.output_dir, args.input_dir)
        else:
            print(f"Error: File {rst_file} not found")
    else:
        # Process all files in the directory
        process_directory(args.input_dir, args.output_dir)

    # Copy images to output directories
    if not args.no_images:
        copy_images_to_output(args.output_dir, args.input_dir)
