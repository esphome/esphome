#!/usr/bin/env python3
import os
import re
import json
from collections import defaultdict

def generate_slug(text):
    slug = re.sub(r"[^\w\s-]", "", text).lower()
    slug = re.sub(r"[-\s]+", "-", slug).strip("-")
    return slug

def process_markdown_file(file_path):
    anchors = []
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        line = lines[i].rstrip()

        # Headings
        heading_match = re.match(r"^(#{1,6})\s+(.*)", line)
        if heading_match:
            heading_text = heading_match.group(2).strip()
            anchor_id = generate_slug(heading_text)
            anchors.append({
                "id": anchor_id,
                "text": heading_text,
                "type": "heading"
            })
            i += 1
            continue

        # Shortcodes
        shortcode_match = re.search(r'\{\{<\s*anchor\s+"([^\s>]+)"\s*>}}', line)
        if shortcode_match:
            anchor_id = shortcode_match.group(1)
            # Look ahead for heading
            description = ""
            j = i + 1
            while j < len(lines):
                next_line = lines[j].strip()
                if next_line == "":
                    j += 1
                    continue
                heading_match = re.match(r"^(#{1,6})\s+(.*)", next_line)
                if heading_match:
                    description = heading_match.group(2).strip()
                break
            anchors.append({
                "id": anchor_id,
                "text": description if description else anchor_id,
                "type": "shortcode"
            })
            i += 1
            continue

        i += 1

    return anchors

def get_page_name(file_path, root_dir):
    rel_path = os.path.relpath(file_path, root_dir)
    dir_name, file_name = os.path.split(rel_path)
    if file_name == "_index.md" and not dir_name:
        # Exclude the top-level _index.md
        return None
    if file_name == "_index.md":
        page_name = dir_name if dir_name else ""
    else:
        base_name = os.path.splitext(file_name)[0]
        page_name = os.path.join(dir_name, base_name) if dir_name else base_name
    # Remove any leading slashes and normalize
    return page_name.strip("/").replace("\\", "/")

def main():
    root_dir = "content"
    output_file = "data/anchors.json"

    # Collect all anchors by page
    page_anchors = {}
    for dirpath, _, filenames in os.walk(root_dir):
        for filename in filenames:
            if filename.endswith(".md"):
                file_path = os.path.join(dirpath, filename)
                page_name = get_page_name(file_path, root_dir)
                if page_name is None:
                    continue  # skip top-level _index.md
                anchors = process_markdown_file(file_path)
                if anchors:
                    page_anchors[page_name] = anchors

    # Collate anchors by ID
    collated = defaultdict(list)
    for page, anchors in page_anchors.items():
        for anchor in anchors:
            entry = {
                "page": page,
                "text": anchor["text"],
                "type": anchor["type"]
            }
            collated[anchor["id"]].append(entry)

    # Sorting logic for pages
    sort_order = ["components", "cookbook", "changelog"]
    def page_sort_key(entry):
        """
        Sort entries so that page filenames are the highest priority
        :param entry:
        :return:
        """
        page = entry["page"]
        if page in sort_order:
            index =  -sort_order.index(page)  # descending order
        else:
            index = -(len(sort_order) + 1)  # others at the end
        return (1 if entry["type"] == "heading" else 0, index)

    # Sort entries within each anchor group
    sorted_collated = {}
    for anchor_id, entries in collated.items():
        entries.sort(key=page_sort_key)
        sorted_collated[anchor_id] = entries

    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(sorted_collated, f, indent=2, ensure_ascii=False)

    print(f"Collated anchor data written to {output_file}")

if __name__ == "__main__":
    main()
