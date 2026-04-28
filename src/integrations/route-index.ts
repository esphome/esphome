import type { AstroIntegration } from "astro";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

type Route = { url: string; title: string };

function getTitle(filePath: string): string | null {
  const content = fs.readFileSync(filePath, "utf-8");
  const match = content.match(/^title:\s*["']?([^"'\n]+)["']?/m);
  return match ? match[1].trim() : null;
}

function walk(dir: string, urlPath: string, out: Route[]): void {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    if (entry.name.startsWith(".") || entry.name === "images") continue;
    const abs = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      walk(abs, `${urlPath}/${entry.name}`, out);
    } else if (entry.isFile() && entry.name.endsWith(".mdx")) {
      const slug = entry.name.replace(/\.mdx$/, "");
      if (slug === "404") continue;
      const title = getTitle(abs);
      const url = slug === "index" ? `${urlPath}/` : `${urlPath}/${slug}/`;
      out.push({
        url: url.replace(/^\/+/, "/"),
        title: title || slug,
      });
    }
  }
}

function collectRoutes(docsDir: string): Route[] {
  const out: Route[] = [];
  walk(docsDir, "", out);
  // Changelog entries are noisy for "did you mean" lookups — keep them out.
  return out.filter((r) => !r.url.startsWith("/changelog"));
}

/**
 * Exposes the full list of docs routes as a virtual module so the 404 page
 * can offer "did you mean" suggestions derived from the requested path.
 *
 * Reads `src/content/docs` at `astro:config:setup` using the Astro-supplied
 * project root, so it works consistently in dev, build and prerender without
 * relying on `process.cwd()` or `import.meta.url`.
 */
export default function routeIndex(): AstroIntegration {
  const VIRTUAL_ID = "virtual:esphome-route-index";
  const RESOLVED_ID = "\0" + VIRTUAL_ID;

  return {
    name: "route-index",
    hooks: {
      "astro:config:setup": ({ config, updateConfig }) => {
        const rootDir = fileURLToPath(config.root);
        const docsDir = path.join(rootDir, "src/content/docs");
        const routes = collectRoutes(docsDir);
        const payload = `export const allRoutes = ${JSON.stringify(routes)};\n`;

        updateConfig({
          vite: {
            plugins: [
              {
                name: "esphome-route-index",
                enforce: "pre",
                resolveId(id: string) {
                  if (id === VIRTUAL_ID) return RESOLVED_ID;
                  return null;
                },
                load(id: string) {
                  if (id === RESOLVED_ID) return payload;
                  return null;
                },
              },
            ],
          },
        });
      },
    },
  };
}
