import Slugger from "github-slugger";
import { visit } from "unist-util-visit";

// ESPHome action/condition/trigger headings contain dots (e.g. `light.turn_on`).
// github-slugger drops `.` entirely, producing mashed anchors like `lightturn_on`.
// This plugin pre-assigns heading ids with `.` mapped to `-` so the anchor becomes
// `light-turn_on`. It runs before Astro's rehypeHeadingIds, which then reuses the
// id we set for both the element and the table-of-contents metadata.
//
// Text extraction mirrors @astrojs/markdown-remark's rehype-collect-headings so the
// only difference from the default slug is the `.` -> `-` mapping.

const rawNodeTypes = new Set(["text", "raw", "mdxTextExpression"]);
const codeTagNames = new Set(["code", "pre"]);

export function rehypeHeadingSlugs() {
  return function (tree, file) {
    const isMDX = Boolean(file.history[0]?.endsWith(".mdx"));
    const slugger = new Slugger();
    visit(tree, (node) => {
      if (node.type !== "element") return;
      const { tagName } = node;
      if (tagName[0] !== "h") return;
      if (!/^h[1-6]$/.test(tagName)) return;
      let text = "";
      visit(node, (child, _index, parent) => {
        if (child.type === "element" || parent == null) return;
        if (child.type === "raw" && /^\n?<.*>\n?$/.test(child.value)) return;
        if (rawNodeTypes.has(child.type)) {
          if (isMDX || codeTagNames.has(parent.tagName)) {
            text += child.value;
          } else {
            text += child.value.replace(/\{/g, "${");
          }
        }
      });
      node.properties = node.properties || {};
      if (typeof node.properties.id !== "string") {
        node.properties.id = slugger.slug(text.replace(/\./g, "-"));
      }
    });
  };
}
