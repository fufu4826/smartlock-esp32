const fs=require('fs'),assert=require('assert'),vm=require('vm');
const source=fs.readFileSync('src/web/WebAssets.h','utf8');
const html=source.match(/static const char kManage\[\] PROGMEM = R"HTML\(([\s\S]*?)\)HTML";/)[1];
const code=html.match(/<script>([\s\S]*?)<\/script>/)[1];new vm.Script(code);
const ids=new Set([...html.matchAll(/\bid="([^"]+)"/g)].map(m=>m[1]));
for(const m of code.matchAll(/getElementById\('([^']+)'\)/g))assert(ids.has(m[1]),'dead control '+m[1]);
for(const id of ['backupForm','restoreValidateForm','restoreActivateForm','prepareOta','otaUploadForm','downloadCurrentFirmware','ownerMaintenance'])assert(!html.includes(id));
assert(html.includes('adminPinForm'));assert(code.includes("window.addEventListener('hashchange',switchManagementPage)"));assert(code.includes("document.getElementById('refreshSystem').addEventListener"));
const backend=fs.readFileSync('src/network/WebServerManager.cpp','utf8');assert(!/system(?:Backup|Restore|Ota)/.test(backend));assert(backend.includes('ownerPinAuthorized()'));
const parser=fs.readFileSync('lib/SmartLockWebServer/src/Parsing.cpp','utf8');assert(!parser.includes('/api/system/restore'));assert(!parser.includes('/api/system/ota'));
console.log('Cleanup Management DOM/control/routes/parser checks PASS');

for(const value of ['lockSettingsPanel','displayPanel','lock-settings'])assert(!html.includes(value));
const links=[...html.matchAll(/data-page-link="([^"]+)"/g)].map(m=>m[1]);
assert.deepEqual(links,['dashboard','management','network','line','unlock','admin-pin','system']);
for(const key of links)assert(code.includes(key+':')||code.includes("'"+key+"':"),'navigation route missing '+key);
const access=fs.readFileSync('src/app/AccessController.cpp','utf8'),main=fs.readFileSync('src/main.cpp','utf8');
assert(access.includes('unlockDurationMs'));assert(main.includes('lockController.unlock(configStore.config().unlockDurationMs)'));
console.log('Management trim navigation and stored duration paths PASS');
