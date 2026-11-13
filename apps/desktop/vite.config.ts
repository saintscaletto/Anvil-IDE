import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

const classicBuild = {
  name: "anvil-classic-build",
  apply: "build" as const,
  transformIndexHtml(html: string) {
    return html
      .replace('<script type="module" crossorigin ', '<script defer ')
      .replace(' crossorigin href=', " href=");
  },
};

export default defineConfig({
  plugins: [react(), classicBuild],
  base: "./",
  build: {
    target: "es2020",
    outDir: "dist",
    assetsDir: "assets",
    assetsInlineLimit: 0,
    sourcemap: false,
    cssCodeSplit: false,
    reportCompressedSize: false,
    rollupOptions: {
      output: {
        format: "iife",
        inlineDynamicImports: true,
        entryFileNames: "assets/[name].js",
        chunkFileNames: "assets/[name].js",
        assetFileNames: "assets/[name][extname]",
      },
    },
  },
  server: {
    port: 5173,
    strictPort: true,
  },
});
