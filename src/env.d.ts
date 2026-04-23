/// <reference path="../.astro/types.d.ts" />
/// <reference types="@astrojs/starlight/virtual-internal" />

declare module "virtual:esphome-route-index" {
  export const allRoutes: { url: string; title: string }[];
}
