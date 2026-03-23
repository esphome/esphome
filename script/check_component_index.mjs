#!/usr/bin/env node
/**
 * Check (and optionally fix) alphabetical ordering of ImgTable blocks
 * in the components index page.
 *
 * Only operates on tables below the "## Network Protocols" heading.
 * Within each table the pinned order is:
 *   1. "Core" item   — at most one, identified by name ending with " Core".
 *      When multiple Core items exist in a table, the one matching the
 *      section keyword is chosen (e.g. "Display Core" over "Display Menu Core"
 *      in "Display Components").
 *   2. "Template" items — names starting with "Template " (e.g. "Template Sensor")
 * All remaining items must be sorted case-insensitively by display name.
 *
 * Usage:
 *   node script/check_component_index.mjs                # check only (exit 1 if unsorted)
 *   node script/check_component_index.mjs --fix          # rewrite the file in-place
 *   node script/check_component_index.mjs --suggestions  # output JSON for CI review comments
 *   node script/check_component_index.mjs --file <path>  # check a specific file (combine with above)
 */

import { readFileSync, writeFileSync } from "fs";
import { join, dirname } from "path";
import { fileURLToPath } from "url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

const INDEX_REL = "src/content/docs/components/index.mdx";
const INDEX_PATH = join(__dirname, "..", INDEX_REL);
const SORT_AFTER_HEADING = "## Network Protocols";

// ── helpers ──────────────────────────────────────────────────────────────────

/** Extract the display name from a raw item line, e.g. '  ["Foo", …],' → 'Foo' */
function getName(line) {
  const m = line.match(/\["([^"]+)"/);
  return m ? m[1] : "";
}

/**
 * Derive the section keyword from surrounding headings.
 *
 * Strips common suffixes like " Components" and " Hardware Platforms"
 * from the parent `##` heading to get the base keyword used for
 * disambiguating multiple Core items (e.g. "Display Components" → "Display").
 *
 * @param {string} parentHeading The nearest `##` heading
 * @returns {string} Keyword for matching
 */
function sectionKeyword(parentHeading) {
  return parentHeading
    .replace(/\s+Components$/i, "")
    .replace(/\s+Hardware\s+Platforms$/i, "");
}

/**
 * Identify the single "Core" entry in a table, if any.
 *
 * Looks for items whose display name ends with " Core" (preceded by
 * whitespace to exclude product names like "iAQ-Core").
 *
 * - If exactly one item matches, it is the core item.
 * - If multiple items match (e.g. "Display Core" and "Display Menu Core"
 *   in the Display Components table), the one whose prefix best matches
 *   the section keyword from the parent `##` heading is chosen.
 *   Specifically, we look for `<keyword> Core` where keyword is derived
 *   from the parent heading (e.g. "Display Components" → "Display").
 * - If none match, returns -1.
 *
 * @param {string[]} lines          Item lines from the ImgTable block
 * @param {string}   parentHeading  Nearest ## heading
 * @returns {number} Index of the core item, or -1
 */
function identifyCoreItem(lines, parentHeading) {
  const candidates = [];
  for (let i = 0; i < lines.length; i++) {
    const name = getName(lines[i]);
    if (/\sCore$/i.test(name)) {
      candidates.push({ index: i, name });
    }
  }

  if (candidates.length === 0) return -1;
  if (candidates.length === 1) return candidates[0].index;

  // Multiple Core items — disambiguate using the section keyword.
  // e.g. keyword "Display" matches "Display Core" over "Display Menu Core".
  const keyword = sectionKeyword(parentHeading).toLowerCase();
  const expected = `${keyword} core`;
  const exactMatch = candidates.find(
    (c) => c.name.toLowerCase() === expected,
  );
  if (exactMatch) return exactMatch.index;

  // Fallback: first candidate (shouldn't normally happen)
  return candidates[0].index;
}

/**
 * Decide whether an item is a "Template" entry.
 *
 * A template item's display name starts with "Template " (e.g.
 * "Template Sensor", "Template Switch"). These are pinned right
 * after Core items in each table, before the alphabetically sorted
 * remainder.
 */
function isTemplateItem(name) {
  return /^Template\s/i.test(name);
}

/** Normalize an item line to consistent 2-space indent + trailing comma */
function normalizeLine(line) {
  const stripped = line.trim().replace(/,$/, "");
  return `  ${stripped},`;
}

// ── main logic ───────────────────────────────────────────────────────────────

function processFile(content, path) {
  const headingIdx = content.indexOf(SORT_AFTER_HEADING);
  if (headingIdx === -1) {
    console.error(`Could not find "${SORT_AFTER_HEADING}" in ${path}`);
    process.exit(2);
  }

  const before = content.slice(0, headingIdx);
  const after = content.slice(headingIdx);
  // Line number where the "after" region starts (1-indexed)
  const afterStartLine = before.split("\n").length;

  const tableRe = /(<ImgTable items=\{\[\n)([\s\S]*?)(\]\} \/>)/g;

  const results = []; // { section, startLine, endLine, original, fixed }
  let tableIndex = 0;

  const replaced = after.replace(
    tableRe,
    (match, prefix, itemsText, suffix, offset) => {
      tableIndex++;

      // Section heading for context
      const textBefore = after.slice(0, offset);
      const headingMatch = textBefore.match(/^(#{2,3}) (.+)$/gm);
      const sectionName = headingMatch
        ? headingMatch[headingMatch.length - 1].replace(/^#+\s*/, "")
        : `table #${tableIndex}`;

      // Extract the nearest ## (parent) heading for Core item disambiguation
      const h2Headings = headingMatch
        ? headingMatch.filter((h) => h.startsWith("## ") && !h.startsWith("### "))
        : [];
      const parentHeading = h2Headings.length > 0
        ? h2Headings[h2Headings.length - 1].replace(/^#+\s*/, "")
        : sectionName;

      // Parse item lines (skip blanks)
      const allItemLines = itemsText.split("\n");
      const lines = allItemLines.filter((l) => l.trim().length > 0);

      if (lines.length <= 1) return match;

      // Core item stays first (at most one), then Template items, then the rest sorted
      const coreIdx = identifyCoreItem(lines, parentHeading);
      const coreLines = coreIdx >= 0 ? [lines[coreIdx]] : [];
      const templateLines = lines.filter(
        (_, i) => i !== coreIdx && isTemplateItem(getName(lines[i])),
      );
      const restLines = lines.filter(
        (_, i) => i !== coreIdx && !isTemplateItem(getName(lines[i])),
      );

      const sorted = [...restLines].sort((a, b) =>
        getName(a).toLowerCase().localeCompare(getName(b).toLowerCase()),
      );

      const expectedOrder = [...coreLines, ...templateLines, ...sorted];
      const isFullyOrdered = lines.every(
        (line, i) => getName(line) === getName(expectedOrder[i]),
      );

      if (!isFullyOrdered) {
        // Compute 1-indexed line numbers in the original file.
        // offset is the position within `after`; the items text starts
        // after the prefix (<ImgTable items={[\n).
        const prefixLines = prefix.split("\n").length - 1; // lines consumed by prefix
        const linesBeforeInAfter = after.slice(0, offset).split("\n").length - 1;
        const startLine = afterStartLine + linesBeforeInAfter + prefixLines;
        const endLine = startLine + allItemLines.filter((l) => l.trim().length > 0).length - 1;

        const originalBlock = lines.map(normalizeLine).join("\n");
        const fixedBlock = [...coreLines, ...templateLines, ...sorted]
          .map(normalizeLine)
          .join("\n");

        results.push({
          section: sectionName,
          startLine,
          endLine,
          original: originalBlock,
          fixed: fixedBlock,
        });
      }

      // Build the fixed version (used by --fix)
      const fixedResult = [...coreLines, ...templateLines, ...sorted]
        .map(normalizeLine)
        .join("\n");

      return `${prefix}${fixedResult}\n${suffix}`;
    },
  );

  return { before, after: replaced, results };
}

// ── entry point ──────────────────────────────────────────────────────────────

const mode = process.argv.includes("--fix")
  ? "fix"
  : process.argv.includes("--suggestions")
    ? "suggestions"
    : "check";

const fileArgIdx = process.argv.indexOf("--file");
const filePath =
  fileArgIdx >= 0 && process.argv[fileArgIdx + 1]
    ? process.argv[fileArgIdx + 1]
    : INDEX_PATH;

const content = readFileSync(filePath, "utf-8");
const { before, after, results } = processFile(content, filePath);

if (results.length === 0) {
  console.log("All ImgTable blocks are correctly sorted.");
  process.exit(0);
}

switch (mode) {
  case "fix":
    writeFileSync(filePath, before + after, "utf-8");
    console.log(`Fixed ${results.length} ImgTable block(s) in ${filePath}`);
    process.exit(0);
    break;

  case "suggestions":
    // Output JSON consumed by the GitHub Actions workflow.
    // Each entry has the file path, line range, and the suggested replacement.
    console.log(
      JSON.stringify({
        file: INDEX_REL,
        suggestions: results.map((r) => ({
          section: r.section,
          startLine: r.startLine,
          endLine: r.endLine,
          body: r.fixed,
        })),
      }),
    );
    process.exit(1);
    break;

  default:
    console.error(
      "Component index ImgTable blocks are not sorted correctly:\n",
    );
    for (const r of results) {
      console.error(`  ${r.section} (lines ${r.startLine}-${r.endLine})`);
    }
    console.error(
      "\nRun `node script/check_component_index.mjs --fix` to fix automatically.",
    );
    process.exit(1);
}
