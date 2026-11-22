/* Shared remote controls, compact layout, custom menus and offline guards. */
const {chromium,webkit}=require('../.tools/browser-test/node_modules/playwright-core');
const assert=require('assert/strict'),fs=require('node:fs');
(async()=>{
 fs.mkdirSync('.tools/review22-17',{recursive:true});
 for(const engine of [chromium,webkit]){
  const browser=await engine.launch({headless:true,executablePath:engine===webkit?process.env.LOCALAPPDATA+'/ms-playwright/webkit-2370/Playwright.exe':'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
  try{for(const transport of ['lan','ble']){
   const context=await browser.newContext({locale:'zh-CN',viewport:{width:1366,height:1000}}),page=await context.newPage(),errors=[];
   page.on('pageerror',e=>errors.push(e.message));
   await page.goto('http://127.0.0.1:4173/console.html?transport='+transport);
   await page.waitForFunction(()=>online&&libraryState&&deviceSettings&&!busy&&!polling);
   await page.evaluate(()=>showPage('settings'));await page.waitForFunction(()=>remoteState&&!remoteBusy);
   assert.equal(await page.locator('#remoteBattery').isVisible(),false);
   await page.locator('#remoteScan').click();await page.waitForFunction(()=>remoteState.devices.length===1&&!remoteState.scanning);
   const searchLayout=await page.evaluate(()=>{const h=$('remotePairDialog').querySelector('h3').getBoundingClientRect(),b=$('remoteSearchAgain').getBoundingClientRect();return {right:b.left>h.right,aligned:Math.abs(h.y+h.height/2-b.y-b.height/2)<2,footer:!!$('remoteSearchAgain').closest('.dialog-footer'),signal:$('remoteDevices').textContent};});
   assert(searchLayout.right&&searchLayout.aligned&&!searchLayout.footer&&searchLayout.signal.includes('-48 dBm'),'Refresh in the dialog header and actual signal shown');
   await page.locator('#remoteDevices button').click();
   await page.waitForFunction(()=>$('remotePairDialog').open&&$('remoteSearchState').textContent.includes('连接')&&!!document.querySelector('#remoteDevices .activity-spinner'));
   await page.waitForFunction(()=>remoteState.connected&&!remoteState.ready);
   assert(await page.locator('#remotePairDialog').evaluate(el=>el.open),'GATT link alone must not close before HID is ready');
   await page.waitForFunction(()=>remoteState.ready&&!$('remotePairDialog').open);
   assert.equal(await page.locator('#remoteBatteryText').textContent(),'74%');
   assert(await page.locator('.remote-battery-track').evaluate(el=>el.getBoundingClientRect().width<=26&&el.getBoundingClientRect().height<=12),'Battery glyph stays compact');
   await page.locator('#remoteSingle').locator('..').locator('.select-trigger').click();
   assert(await page.locator('#remoteSingleMenu').evaluate(el=>el.matches(':popover-open')));
   await page.locator('#remoteSingleMenu [data-value="3"]').click();
   await page.waitForFunction(()=>!remoteBusy&&remoteState.singleClick===3);
   for(const language of ['zh-CN','en'])for(const width of [1366,390,320])for(const theme of ['light','dark']){
    await page.setViewportSize({width,height:1000});
    await page.evaluate(({language,theme})=>{applyLanguage(language);document.documentElement.dataset.theme=theme;renderRemote();},{language,theme});
    const geo=await page.locator('#bluetoothRemoteCard').evaluate(el=>{
     const box=n=>n.getBoundingClientRect().toJSON(),heading=box(el.querySelector('h3')),scan=box(document.querySelector('#remoteScan'));
     const row=el.querySelector('.remote-summary'),options=el.querySelector('.remote-options'),mapping=el.querySelector('.remote-mappings'),actions=el.querySelector('.wifi-actions');
     return {card:box(el),heading,scan,summary:box(row),options:box(options),mapping:box(mapping),actions:box(actions),scroll:document.documentElement.scrollWidth,width:innerWidth,
      controls:[...el.querySelectorAll('.select-trigger')].map(x=>({box:box(x),width:x.clientWidth,scroll:x.scrollWidth}))};
    });
    assert(geo.card.left>=0&&geo.card.right<=width+1&&geo.scroll<=width+1,JSON.stringify(geo));
    assert(geo.summary.bottom<=geo.options.top&&geo.options.bottom<=geo.mapping.top&&geo.mapping.bottom<=geo.actions.top,'Rows remain vertically ordered');
    assert(Math.abs(geo.heading.top+geo.heading.height/2-geo.scan.top-geo.scan.height/2)<2,'Search aligns with title');
    assert(geo.scan.left>geo.card.left+geo.card.width/2,'Search stays on the right');
    for(const c of geo.controls)assert(c.scroll<=c.width+1&&c.box.left>=geo.card.left&&c.box.right<=geo.card.right,'Complete mapping label fits');
    if(engine===chromium&&transport==='lan'&&width===390&&theme==='dark')await page.waitForTimeout(250);
    if(engine===chromium&&transport==='lan'&&width===390&&theme==='dark')await page.locator('#bluetoothRemoteCard').screenshot({path:'.tools/review22-17/remote-card-'+language+'.png'});
   }
   const low=await page.evaluate(async()=>{
    const notices=[];message=(...args)=>notices.push(args);
    TP_DEMO.setRemote({battery:20});remoteReadAt=0;await readRemote(true);renderRemote();renderRemote();const once=notices.length;
    TP_DEMO.setRemote({battery:19});remoteReadAt=0;await readRemote(true);const same=notices.length;
    TP_DEMO.setRemote({battery:60});remoteReadAt=0;await readRemote(true);
    TP_DEMO.setRemote({battery:10});remoteReadAt=0;await readRemote(true);return {once,same,total:notices.length};
   });assert.deepEqual(low,{once:1,same:1,total:2});
   await page.locator('#remoteDisconnect').click();await page.waitForFunction(()=>!remoteBusy&&!remoteState.ready);
   assert.equal(await page.locator('#remoteBattery').isVisible(),false);
   await page.locator('#remoteForget').click();await page.waitForFunction(()=>$('confirmDialog').open);
   await page.evaluate(()=>$('confirmDialog').close('cancel'));assert.equal(await page.evaluate(()=>remoteState.paired),true);
   const offline=await page.evaluate(()=>{
    online=false;buttons();renderRemote();
    return {fields:[...document.querySelectorAll('[data-bar-dialog]')].map(b=>{b.click();return {disabled:b.disabled,open:$(b.dataset.barDialog).open};}),remote:[...remoteCard.querySelectorAll('.select-trigger')].map(b=>b.disabled)};
   });assert(offline.fields.every(x=>x.disabled&&!x.open)&&offline.remote.every(Boolean),'Offline dialogs and remote mappings are disabled');
   assert.deepEqual(errors,[]);console.log('PASS',engine.name(),transport,'pairing/battery/custom menus/ordered rows/title alignment/320px bilingual light-dark/low battery dedup/offline dialogs/disconnect/forget confirmation');
   await context.close();
  }}finally{await browser.close();}
 }
})().catch(e=>{console.error(e);process.exitCode=1});

