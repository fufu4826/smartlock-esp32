const fs = require('fs');
const vm = require('vm');
const crypto = require('crypto');

const source = fs.readFileSync('src/web/WebAssets.h', 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/);
if (!script) throw new Error('Wizard script missing');
new vm.Script(script[1]);

const values = {
  deviceName: 'TestLock', ownerName: 'TestOwner', browserName: 'TestBrowser',
  adminPassphrase: 'test-passphrase-123', unlockSeconds: '5'
};
const elements = new Map();
const handlers = {};
function element(id) {
  if (!elements.has(id)) {
    elements.set(id, {
      value: values[id] || '', hidden: false, disabled: false, textContent: '',
      addEventListener(name, handler) { handlers[`${id}:${name}`] = handler; },
      reportValidity() { return true; }, reset() {}
    });
  }
  return elements.get(id);
}
const entries = new Map();
const localStorage = {
  getItem(key) { return entries.has(key) ? entries.get(key) : null; },
  setItem(key, value) { entries.set(key, value); },
  removeItem(key) { entries.delete(key); }
};
let request;
const context = {
  document: {getElementById: element},
  window: {location: {search: `?session=${'a'.repeat(64)}`}, crypto: crypto.webcrypto},
  crypto: crypto.webcrypto, localStorage, TextEncoder, URLSearchParams,
  fetch: async (url, options) => {
    request = {url, options};
    return {status: 200, json: async () => ({ok: true, apSsid: 'SmartLock-TEST'})};
  }
};
vm.runInNewContext(script[1], context);

(async () => {
  await handlers['setupForm:submit']({preventDefault() {}});
  if (!request || request.url !== '/api/setup/complete') throw new Error('Setup POST missing');
  const body = new URLSearchParams(request.options.body);
  if (!/^[0-9a-f]{64}$/.test(body.get('credential')) ||
      !/^[0-9a-f]{32}$/.test(body.get('apPassword'))) throw new Error('Random credential format');
  const saved = JSON.parse(localStorage.getItem('smartlock.setup.v1'));
  if (saved.state !== 'active' || saved.credential !== body.get('credential') ||
      saved.apPassword !== body.get('apPassword') ||
      JSON.stringify(saved).includes('test-passphrase-123')) throw new Error('Active browser storage');
  localStorage.removeItem('smartlock.setup.v1');
  if (localStorage.getItem('smartlock.setup.v1') !== null) throw new Error('Storage clear failed');
  element('adminPassphrase').value = 'test-passphrase-123';
  context.fetch = async () => ({status: 503});
  vm.runInNewContext(script[1], context);
  await handlers['setupForm:submit']({preventDefault() {}});
  const pending = JSON.parse(localStorage.getItem('smartlock.setup.v1'));
  if (pending.state !== 'pending' || !/^[0-9a-f]{64}$/.test(pending.payload.credential) ||
      JSON.stringify(pending).includes('test-passphrase-123')) throw new Error('Pending browser storage');
  process.stdout.write('Wizard random values, active/pending storage, and credential loss on clear: PASS\n');
})().catch((error) => { process.stderr.write(`${error}\n`); process.exitCode = 1; });
