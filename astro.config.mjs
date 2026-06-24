import { defineConfig } from "astro/config";
import starlight from "@astrojs/starlight";
import sitemap from "@astrojs/sitemap";
import { fileURLToPath } from "url";
import path from "path";
import fs from "fs";
import { imageBreakpoints } from "./src/lib/breakpoints.ts";
import { remarkAlert } from "remark-github-blockquote-alert";
import remarkMath from "remark-math";
import rehypeKatex from "rehype-katex";
import { rehypeHeadingSlugs } from "./src/lib/rehype-heading-slugs.mjs";
import componentsJson from "./src/integrations/components-json.ts";
import routeIndex from "./src/integrations/route-index.ts";

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
        const sortValue = parseInt(major) * 10000 + parseInt(minor) * 100 + parseInt(patch);
        const slug = `v${major}.${minor}.${patch}`;
        return { label, slug, sortValue };
      }
      // 2021.x.x format
      const match2 = f.match(/^(\d{4})\.(\d+)\.(\d+)\.mdx$/);
      if (match2) {
        const [, year, month, patch] = match2;
        const sortValue = parseInt(year) * 10000 + parseInt(month) * 100 + parseInt(patch);
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

      let groupLabel = entry.name.replace(/_/g, " ").replace(/\b\w/g, (c) => c.toUpperCase());
      if (fs.existsSync(indexPath)) {
        const content = fs.readFileSync(indexPath, "utf-8");
        const titleMatch = content.match(/^title:\s*["']?([^"'\n]+)["']?/m);
        if (titleMatch) groupLabel = titleMatch[1];
      }

      const subFiles = fs.readdirSync(dirPath).filter((f) => f.endsWith(".mdx"));
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

export default defineConfig({
  site: "https://esphome.io",
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
    // Astro 6 no longer defaults `markdown.gfm` to true, and @astrojs/mdx only applies remark-gfm
    // to .mdx files when this is explicitly truthy. Without it, GFM tables render as literal text.
    gfm: true,
    remarkPlugins: [remarkAlert, remarkMath],
    rehypePlugins: [rehypeHeadingSlugs, rehypeKatex],
  },
  integrations: [
    starlight({
      title: "ESPHome - Smart Home Made Simple",
      titleDelimiter: "-",
      favicon: "/favicon.ico",
      pagination: false,
      plugins: [],
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
        baseUrl: `https://github.com/esphome/esphome.io/edit/${
          ["next", "beta"].includes(process.env.BRANCH) ? "next" : "current"
        }/`,
      },
      routeMiddleware: ["./src/routeData.ts"],
      components: {
        Footer: "./src/components/Footer.astro",
        Head: "./src/components/Head.astro",
        SiteTitle: "./src/components/SiteTitle.astro",
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
          items: [{ autogenerate: { directory: "automations", collapsed: true } }],
        },
        {
          label: "Guides",
          collapsed: true,
          items: [{ autogenerate: { directory: "guides", collapsed: true } }],
        },
        {
          label: "Cookbook",
          collapsed: true,
          items: [{ autogenerate: { directory: "cookbook", collapsed: true } }],
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
            // 1. Existing '/' shortcut to open search
            if (e.key === '/' && !['INPUT', 'TEXTAREA', 'SELECT'].includes(document.activeElement?.tagName)) {
              e.preventDefault();
              document.querySelector('button[data-open-modal]')?.click();
              return;
            }

            // 2. New 'Enter' shortcut for first search result
            if (e.key === 'Enter' && e.target.classList.contains('pagefind-ui__search-input')) {
              const firstResult = document.querySelector('.pagefind-ui__result-link');

              if (firstResult) {
                // Prevent the default form submission or modal close behavior
                e.preventDefault();
                e.stopImmediatePropagation();
                firstResult.click();
              }
            }
          });

          function fallbackCopyText(text) {
            return new Promise(function(resolve, reject) {
              try {
                const textarea = document.createElement('textarea');
                textarea.value = text;
                textarea.setAttribute('readonly', '');
                textarea.style.position = 'fixed';
                textarea.style.top = '-9999px';
                textarea.style.left = '-9999px';
                document.body.appendChild(textarea);
                textarea.focus();
                textarea.select();

                const copied = document.execCommand('copy');
                textarea.remove();

                if (copied) {
                  resolve();
                } else {
                  reject(new Error('Fallback copy failed'));
                }
              } catch (err) {
                reject(err);
              }
            });
          }

          function copyText(text) {
            if (navigator.clipboard && typeof navigator.clipboard.writeText === 'function') {
              return navigator.clipboard.writeText(text).catch(function() {
                return fallbackCopyText(text);
              });
            }

            return fallbackCopyText(text);
          }

          document.addEventListener('click', function(e) {
            if (!(e.target instanceof Element)) return;
            const anchor = e.target.closest('a.sl-anchor-link');
            if (!anchor) return;
            if (
              e.button !== 0 ||
              e.metaKey ||
              e.ctrlKey ||
              e.shiftKey ||
              e.altKey
            ) {
              return;
            }
            e.preventDefault();
            const url = anchor.href;
            copyText(url).then(function() {
              showLinkCopiedToast(anchor);
            }).catch(function() {
              const targetUrl = new URL(anchor.href, window.location.href);
              if (targetUrl.hash) {
                window.location.hash = targetUrl.hash;
              } else {
                window.location.assign(anchor.href);
              }
            });
          });

          function showLinkCopiedToast(anchor) {
            const existing = document.getElementById('sl-link-toast');
            if (existing) existing.remove();
            const rect = anchor.getBoundingClientRect();

            const toast = document.createElement('div');
            toast.id = 'sl-link-toast';
            toast.setAttribute('role', 'status');
            toast.setAttribute('aria-live', 'polite');
            toast.setAttribute('aria-atomic', 'true');
            toast.textContent = 'Link copied to clipboard';
            toast.style.cssText = [
              'position:fixed',
              'top:' + (rect.top + rect.height / 2 - 14) + 'px',
              'left:' + (rect.right + 8) + 'px',
              'background:var(--sl-color-gray-1)',
              'color:var(--sl-color-gray-6)',
              'padding:0.25rem 0.75rem',
              'border-radius:999px',
              'font-size:0.8rem',
              'white-space:nowrap',
              'box-shadow:0 2px 8px rgba(0,0,0,0.2)',
              'z-index:9999',
              'opacity:1',
              'transition:opacity 0.4s ease',
              'pointer-events:none',
            ].join(';');
            document.body.appendChild(toast);

            // If it overflows the right edge, flip it to the left of the icon
            const toastRect = toast.getBoundingClientRect();
            if (toastRect.right > window.innerWidth - 8) {
              toast.style.left = (rect.left - toastRect.width - 8) + 'px';
            }

            setTimeout(function() {
              toast.style.opacity = '0';
              setTimeout(function() { toast.remove(); }, 400);
            }, 2000);
          }`,
        },
        {
          tag: "meta",
          attrs: {
            property: "og:image",
            content: "https://esphome.io/images/og.webp",
          },
        },
        {
          tag: "meta",
          attrs: {
            name: "twitter:image",
            content: "https://esphome.io/images/og.webp",
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
    componentsJson(),
    routeIndex(),
  ],
});
