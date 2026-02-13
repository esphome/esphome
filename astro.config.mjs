import { defineConfig } from "astro/config";
import starlight from "@astrojs/starlight";
import sitemap from "@astrojs/sitemap";
import starlightBlog from "starlight-blog";
import { fileURLToPath } from "url";
import path from "path";
import fs from "fs";
import { imageBreakpoints } from "./src/lib/breakpoints.ts";
import { remarkAlert } from "remark-github-blockquote-alert";
import remarkMath from "remark-math";
import rehypeKatex from "rehype-katex";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// Generate changelog sidebar items sorted newest to oldest
function getChangelogItems() {
  const changelogDir = path.join(__dirname, "src/content/docs/changelog");
  const files = fs.readdirSync(changelogDir).filter((f) => f.endsWith(".mdx"));

  const versions = files
    .map((f) => {
      const filePath = path.join(changelogDir, f);
      const content = fs.readFileSync(filePath, "utf-8");
      const titleMatch = content.match(/^title:\s*["']?([^"'\n]+)["']?/m);
      const label = titleMatch ? titleMatch[1] : f.replace(".mdx", "");

      // v1.x.x format
      const match = f.match(/^v(\d+)\.(\d+)\.(\d+)\.mdx$/);
      if (match) {
        const [, major, minor, patch] = match;
        const sortValue =
          parseInt(major) * 10000 + parseInt(minor) * 100 + parseInt(patch);
        const slug = `v${major}.${minor}.${patch}`;
        return { label, slug, sortValue };
      }
      // 2021.x.x format
      const match2 = f.match(/^(\d{4})\.(\d+)\.(\d+)\.mdx$/);
      if (match2) {
        const [, year, month, patch] = match2;
        const sortValue =
          parseInt(year) * 10000 + parseInt(month) * 100 + parseInt(patch);
        const slug = `${year}.${month}.${patch}`;
        return { label, slug, sortValue };
      }
      return null;
    })
    .filter(Boolean);

  // Sort descending (newest first)
  versions.sort((a, b) => b.sortValue - a.sortValue);

  return versions.map((v) => ({
    label: v.label,
    link: `/changelog/${v.slug}/`,
  }));
}

// Get the latest changelog version for redirects
function getLatestChangelog() {
  const changelogDir = path.join(__dirname, "src/content/docs/changelog");
  const files = fs
    .readdirSync(changelogDir)
    .filter((f) => f.endsWith(".mdx") && f !== "index.mdx");

  let latest = { sortValue: 0, slug: "" };
  for (const f of files) {
    const match = f.match(/^(\d{4})\.(\d+)\.(\d+)\.mdx$/);
    if (match) {
      const [, year, month, patch] = match;
      const sortValue =
        parseInt(year) * 10000 + parseInt(month) * 100 + parseInt(patch);
      if (sortValue > latest.sortValue) {
        latest = { sortValue, slug: `${year}.${month}.${patch}` };
      }
    }
  }
  return latest.slug;
}

// Generate component sidebar items with proper labels
function getComponentItems() {
  const componentsDir = path.join(__dirname, "src/content/docs/components");
  const entries = fs.readdirSync(componentsDir, { withFileTypes: true });
  const items = [];

  for (const entry of entries) {
    if (entry.name === "images" || entry.name === "index.mdx") continue;

    if (entry.isFile() && entry.name.endsWith(".mdx")) {
      const filePath = path.join(componentsDir, entry.name);
      const content = fs.readFileSync(filePath, "utf-8");
      const titleMatch = content.match(/^title:\s*["']?([^"'\n]+)["']?/m);
      const label = titleMatch ? titleMatch[1] : entry.name.replace(".mdx", "");
      const slug = entry.name.replace(".mdx", "");
      items.push({
        label,
        link: `/components/${slug}/`,
        sort: label.toLowerCase(),
      });
    } else if (entry.isDirectory()) {
      const dirPath = path.join(componentsDir, entry.name);
      const indexPath = path.join(dirPath, "index.mdx");

      let groupLabel = entry.name
        .replace(/_/g, " ")
        .replace(/\b\w/g, (c) => c.toUpperCase());
      if (fs.existsSync(indexPath)) {
        const content = fs.readFileSync(indexPath, "utf-8");
        const titleMatch = content.match(/^title:\s*["']?([^"'\n]+)["']?/m);
        if (titleMatch) groupLabel = titleMatch[1];
      }

      const subFiles = fs
        .readdirSync(dirPath)
        .filter((f) => f.endsWith(".mdx"));
      const subItems = [];
      for (const f of subFiles) {
        const subPath = path.join(dirPath, f);
        const content = fs.readFileSync(subPath, "utf-8");
        const titleMatch = content.match(/^title:\s*["']?([^"'\n]+)["']?/m);
        const label = titleMatch ? titleMatch[1] : f.replace(".mdx", "");
        const slug = f === "index.mdx" ? "" : f.replace(".mdx", "") + "/";
        const item = { label, link: `/components/${entry.name}/${slug}` };
        if (f === "index.mdx") {
          subItems.unshift(item);
        } else {
          subItems.push(item);
        }
      }

      items.push({
        label: groupLabel,
        collapsed: true,
        items: subItems,
        sort: groupLabel.toLowerCase(),
      });
    }
  }

  items.sort((a, b) => a.sort.localeCompare(b.sort));
  return items.map(({ sort, ...item }) => item);
}

const latestChangelog = getLatestChangelog();

export default defineConfig({
  site: "https://esphome.io",
  redirects: {
    "/changelog/": `/changelog/${latestChangelog}/`,
    "/changelog/index/": `/changelog/${latestChangelog}/`,
    "/guides/changelog/": `/changelog/${latestChangelog}/`,
  },
  vite: {
    resolve: {
      alias: {
        "@components": path.resolve(__dirname, "./src/components"),
      },
    },
  },
  image: {
    breakpoints: imageBreakpoints,
    responsiveStyles: true,
  },
  markdown: {
    remarkPlugins: [remarkAlert, remarkMath],
    rehypePlugins: [rehypeKatex],
  },
  integrations: [
    starlight({
      title: "ESPHome - Smart Home Made Simple",
      titleDelimiter: "-",
      favicon: "/favicon.ico",
      pagination: false,
      plugins: [
        //starlightBlog({
        //  title: 'Blog',
        //}),
      ],
      logo: {
        light: "./src/assets/logo-dark.svg",
        dark: "./src/assets/logo-light.svg",
        replacesTitle: true,
      },
      social: [
        {
          icon: "github",
          label: "GitHub",
          href: "https://github.com/esphome/esphome",
        },
        {
          icon: "discord",
          label: "Discord",
          href: "https://discord.gg/KhAMKrd",
        },
      ],
      editLink: {
        baseUrl: "https://github.com/esphome/esphome-docs/edit/current/",
      },
      components: {
        Footer: "./src/components/Footer.astro",
      },
      customCss: ["./src/styles/custom.css", "katex/dist/katex.min.css"],
      sidebar: [
        {
          label: "Getting Started",
          items: [
            {
              label: "From Home Assistant",
              link: "/guides/getting_started_hassio/",
            },
            {
              label: "Using Command Line",
              link: "/guides/getting_started_command_line/",
            },
            { label: "Ready-Made Projects", link: "/projects/" },
            {
              label: "Migrate from Tasmota",
              link: "/guides/migrate_sonoff_tasmota/",
            },
            { label: "FAQ and Tips", link: "/guides/faq/" },
          ],
        },
        { label: "Components", link: "/components/" },
        {
          label: "All Components",
          collapsed: true,
          items: getComponentItems(),
        },
        {
          label: "Automations",
          collapsed: true,
          autogenerate: { directory: "automations" },
        },
        {
          label: "Guides",
          collapsed: true,
          autogenerate: { directory: "guides" },
        },
        {
          label: "Cookbook",
          collapsed: true,
          autogenerate: { directory: "cookbook" },
        },
        {
          label: "Keeping Up",
          items: [
            //{ label: "Blog", link: "/blog/" },
            { label: "Changelog", link: "/changelog/" },
            { label: "Discord", link: "https://discord.gg/KhAMKrd" },
            {
              label: "Forums",
              link: "https://community.home-assistant.io/c/esphome/",
            },
            { label: "Development", link: "https://developers.esphome.io" },
            { label: "Supporters", link: "/guides/supporters/" },
          ],
        },
        {
          label: "Changelog",
          collapsed: true,
          items: getChangelogItems(),
        },
      ],
      head: [
        {
          tag: "link",
          attrs: {
            rel: "icon",
            href: "/favicon.ico",
          },
        },
        {
          tag: "script",
          content: `document.addEventListener('keydown', function(e) {
            if (e.key === '/' && !['INPUT', 'TEXTAREA', 'SELECT'].includes(document.activeElement?.tagName)) {
              e.preventDefault();
              document.querySelector('button[data-open-modal]')?.click();
            }
          });`,
        },
        {
          tag: "meta",
          attrs: {
            property: "og:image",
            content: "https://esphome.io/images/logo.svg",
          },
        },
        {
          tag: "meta",
          attrs: {
            name: "twitter:image",
            content: "https://esphome.io/images/logo.svg",
          },
        },
        {
          tag: "meta",
          attrs: {
            name: "twitter:card",
            content: "summary_large_image",
          },
        },
      ],
    }),
    sitemap(),
  ],
});
