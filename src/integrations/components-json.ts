import type { AstroIntegration } from "astro";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

function getTitle(filePath: string): string | null {
  const content = fs.readFileSync(filePath, "utf-8");
  const match = content.match(/^title:\s*["']?([^"'\n]+)["']?/m);
  return match ? match[1] : null;
}

function getSeoImage(filePath: string): string | null {
  const content = fs.readFileSync(filePath, "utf-8");
  const match = content.match(/^seo:\s*\n\s*image:\s*["']?([^"'\n]+)["']?/m);
  return match ? match[1] : null;
}

const IMAGE_EXTENSIONS = [".png", ".jpg", ".svg", ".webp"];

function findPublicImage(imagesDir: string, slug: string): string | null {
  for (const ext of IMAGE_EXTENSIONS) {
    const candidate = path.join(imagesDir, `${slug}${ext}`);
    if (fs.existsSync(candidate)) {
      return `${slug}${ext}`;
    }
  }
  return null;
}

/**
 * Parse the components index page to build a map from component URL path
 * to the image filename used in the ImgTable entries.
 * e.g. "/components/api" -> "server-network.svg"
 */
function buildIndexImageMap(indexPath: string): Record<string, string> {
  const map: Record<string, string> = {};
  const content = fs.readFileSync(indexPath, "utf-8");
  // Match entries like: ["Label", "/components/api/", "server-network.svg", ...]
  const re = /\["[^"]*",\s*"(\/components\/[^"]+)",\s*"([^"]+)"/g;
  let match;
  while ((match = re.exec(content)) !== null) {
    const url = match[1].replace(/\/$/, "");
    if (!map[url]) {
      map[url] = match[2];
    }
  }
  return map;
}

export default function componentsJson(): AstroIntegration {
  let siteURL = "https://esphome.io";
  let rootDir = "";
  let outDir = "";

  return {
    name: "components-json",
    hooks: {
      "astro:config:done": ({ config }) => {
        if (config.site) {
          siteURL = config.site.replace(/\/$/, "");
        }
        rootDir = fileURLToPath(config.root);
        outDir = fileURLToPath(config.outDir);
      },
      "astro:build:done": async ({ logger }) => {
        const imagesDir = path.join(rootDir, "public/images");
        const componentsDir = path.join(rootDir, "src/content/docs/components");
        const indexImageMap = buildIndexImageMap(path.join(componentsDir, "index.mdx"));

        const components: Record<string, { title: string; url: string; path: string; image?: string }> = {};

        const entries = fs.readdirSync(componentsDir, {
          withFileTypes: true,
        });

        for (const entry of entries) {
          if (entry.name === "images" || entry.name === "index.mdx") continue;

          if (entry.isFile() && entry.name.endsWith(".mdx")) {
            const filePath = path.join(componentsDir, entry.name);
            const title = getTitle(filePath);
            if (!title) continue;

            const slug = entry.name.replace(".mdx", "");
            const seoImage = getSeoImage(filePath);
            const urlPath = `/components/${slug}`;
            const image = seoImage || findPublicImage(imagesDir, slug) || indexImageMap[urlPath];

            components[slug] = {
              title,
              url: `${siteURL}/components/${slug}/`,
              path: `components/${slug}`,
              ...(image && {
                image: `${siteURL}/images/${image}`,
              }),
            };
          } else if (entry.isDirectory()) {
            const dirPath = path.join(componentsDir, entry.name);
            const indexPath = path.join(dirPath, "index.mdx");

            if (fs.existsSync(indexPath)) {
              const title = getTitle(indexPath);
              if (title) {
                const seoImage = getSeoImage(indexPath);
                const urlPath = `/components/${entry.name}`;
                const image = seoImage || findPublicImage(imagesDir, entry.name) || indexImageMap[urlPath];
                components[entry.name] = {
                  title,
                  url: `${siteURL}/components/${entry.name}/`,
                  path: `components/${entry.name}`,
                  ...(image && {
                    image: `${siteURL}/images/${image}`,
                  }),
                };
              }
            }

            const subFiles = fs.readdirSync(dirPath).filter((f) => f.endsWith(".mdx"));
            for (const f of subFiles) {
              if (f === "index.mdx") continue;

              const subPath = path.join(dirPath, f);
              const title = getTitle(subPath);
              if (!title) continue;

              const slug = f.replace(".mdx", "");
              const key = `${entry.name}_${slug}`;
              const seoImage = getSeoImage(subPath);
              const urlPath = `/components/${entry.name}/${slug}`;
              const image =
                seoImage ||
                findPublicImage(imagesDir, slug) ||
                indexImageMap[urlPath] ||
                indexImageMap[`/components/${slug}`] ||
                findPublicImage(imagesDir, entry.name);

              components[key] = {
                title,
                url: `${siteURL}/components/${entry.name}/${slug}/`,
                path: `components/${slug}`,
                ...(image && {
                  image: `${siteURL}/images/${image}`,
                }),
              };
            }
          }
        }

        const outPath = path.join(outDir, "components.json");
        fs.writeFileSync(outPath, JSON.stringify(components, null, 2));
        logger.info(`Generated components.json with ${Object.keys(components).length} components`);
      },
    },
  };
}
