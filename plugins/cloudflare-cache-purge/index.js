/**
 * Netlify Build Plugin: Purge Cloudflare CDN cache after successful deploy.
 *
 * Requires the following environment variables to be set in Netlify:
 *   CLOUDFLARE_API_TOKEN  – API token with "Cache Purge" permission for the zone
 *   CLOUDFLARE_ZONE_ID    – Zone ID of the Cloudflare zone (e.g. esphome.io zone)
 *
 * The hostname to purge is derived from the SITE_URL env var that Netlify
 * sets per deploy context (production / beta / next).
 */

export const onSuccess = async function ({ utils }) {
  const context = process.env.CONTEXT;

  if (context !== "production" && context !== "branch-deploy") {
    console.log(`Skipping Cloudflare cache purge: context is "${context}"`);
    return;
  }

  const apiToken = process.env.CLOUDFLARE_API_TOKEN;
  const zoneId = process.env.CLOUDFLARE_ZONE_ID;
  const siteUrl = process.env.SITE_URL || process.env.URL;

  if (!apiToken || !zoneId) {
    console.log(
      "Skipping Cloudflare cache purge: CLOUDFLARE_API_TOKEN or CLOUDFLARE_ZONE_ID not set",
    );
    return;
  }

  if (!siteUrl) {
    utils.build.failPlugin(
      "Cannot determine hostname: neither SITE_URL nor URL is set",
    );
    return;
  }

  try {
    const hostname = new URL(siteUrl).hostname;

    console.log(`Purging Cloudflare cache for hostname: ${hostname}`);

    const response = await fetch(
      `https://api.cloudflare.com/client/v4/zones/${zoneId}/purge_cache`,
      {
        method: "POST",
        headers: {
          Authorization: `Bearer ${apiToken}`,
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ hosts: [hostname] }),
      },
    );

    const result = await response.json();

    if (!result.success) {
      const errors = result.errors
        .map((e) => `${e.code}: ${e.message}`)
        .join(", ");
      utils.build.failPlugin(`Cloudflare cache purge failed: ${errors}`);
      return;
    }

    console.log(`Cloudflare cache purge successful for ${hostname}`);
    utils.status.show({
      title: "Cloudflare Cache Purge",
      summary: `Purged CDN cache for ${hostname}`,
    });
  } catch (error) {
    utils.build.failPlugin("Cloudflare cache purge failed", { error });
  }
};
