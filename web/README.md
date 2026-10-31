# nod website

Static HTML, local JavaScript, and compiled Tailwind CSS. No client framework, CDN CSS, remote fonts, analytics, or runtime service required.

```sh
cd web
npm ci
npm run build
npm run dev
```

Preview at http://localhost:4173. Publish `web/dist/` to GitHub Pages. The root workflow handles production deployment. All asset paths are relative so the `/nod/` project path works.

Edit `index.html` and `docs.html`, `src/style.css`, and `app.js`. `build.mjs` copies the source pages/assets and compiles/minifies Tailwind CSS. `dist/` and `node_modules/` are ignored. The Tailwind dependency is locked in `package-lock.json`.

The design uses nod's compiled default colors, monospace card headers, borders and notification structure. Both screenshots are copied from the app repository. The homepage's interactive card deck and color selectors are illustrative browser previews; the screenshot section shows the real app. Motion respects `prefers-reduced-motion`; all key content is available without JavaScript.

Canonical URL: https://matteo.merola.co/nod/ (inherited GitHub Pages domain).

Search metadata, SoftwareApplication JSON-LD, sitemap.xml, robots.txt, llms.txt, and pricing.md describe the actual application and its MIT license. Keep version references, documentation date and machine-readable content current when releasing.
