const fs = require('fs');
const path = require('path');

const baseDir = __dirname;
const htmlPath = path.join(baseDir, 'index.html');
const cssPath = path.join(baseDir, 'index.css');
const jsPath = path.join(baseDir, 'app.js');
const outputPath = path.join(baseDir, 'esp_now_receiver', 'dashboard_html.h');

try {
    let html = fs.readFileSync(htmlPath, 'utf8');
    const css = fs.readFileSync(cssPath, 'utf8');
    const js = fs.readFileSync(jsPath, 'utf8');

    // Replace external CSS link with inline styles
    html = html.replace(
        '<link rel="stylesheet" href="index.css" />',
        `<style>\n${css}\n</style>`
    );

    // Replace external JS script with inline script
    html = html.replace(
        '<script src="app.js"></script>',
        `<script>\n${js}\n</script>`
    );

    // Generate the C header content
    const headerContent = `// ─────────────────────────────────────────────────────────────
//  Formula Manipal — Embedded Dashboard HTML (Auto-Generated)
//  Served from ESP32 AP at 192.168.4.1
//  WebSocket on port 81 receives raw 79-byte binary packets
// ─────────────────────────────────────────────────────────────
#pragma once

static const char DASHBOARD_HTML[] PROGMEM = R"HTMLEOF(
${html}
)HTMLEOF";
`;

    fs.writeFileSync(outputPath, headerContent, 'utf8');
    console.log('Successfully bundled index.html, index.css, and app.js into dashboard_html.h');

} catch (err) {
    console.error('Error bundling frontend:', err);
}
