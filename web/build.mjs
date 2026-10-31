import { rmSync, mkdirSync, cpSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
rmSync('dist', { recursive: true, force: true });
mkdirSync('dist/assets', { recursive: true });
for (const file of ['index.html', 'docs.html', 'app.js', 'llms.txt', 'pricing.md', 'robots.txt', 'sitemap.xml', 'favicon.svg', '.nojekyll']) cpSync(file, `dist/${file}`);
cpSync('assets', 'dist/assets', { recursive: true });
execFileSync('node_modules/.bin/tailwindcss', ['-i', 'src/style.css', '-o', 'dist/style.css', '--minify'], { stdio: 'inherit' });
