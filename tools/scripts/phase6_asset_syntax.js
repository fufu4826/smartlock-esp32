const fs = require('fs');
const source = fs.readFileSync('src/web/WebAssets.h', 'utf8');
for (const name of ['Index', 'Setup', 'Manage', 'Enroll', 'Access', 'Reset']) {
  const marker = `static const char k${name}[] PROGMEM = R"HTML(`;
  const start = source.indexOf(marker);
  if (start < 0) throw Error(`Missing ${name}`);
  const end = source.indexOf(')HTML";', start);
  if (end < 0) throw Error(`Unclosed ${name}`);
  const html = source.slice(start + marker.length, end);
  const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)];
  if (scripts.length !== 1) throw Error(`${name} script count`);
  new Function(scripts[0][1]);
  console.log(`${name}: JS syntax PASS`);
}
