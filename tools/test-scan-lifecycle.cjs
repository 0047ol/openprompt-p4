/* Browser lifecycle checks use the shared Demo, not physical radios or SD writes. */
const {chromium,webkit}=require('../.tools/browser-test/node_modules/playwright-core');
const assert=require('node:assert/strict'),fs=require('node:fs');
(async()=>{
 const results=[];
 for(const engine of [chromium,webkit]){
  const browser=await engine.launch({headless:true,executablePath:engine===webkit?process.env.LOCALAPPDATA+'/ms-playwright/webkit-2370/Playwright.exe':'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
  try{for(const transport of ['lan','ble']){
   const context=await browser.newContext({locale:'zh-CN',viewport:{width:390,height:844},hasTouch:true}),page=await context.newPage(),errors=[];
   page.setDefaultTimeout(15000);page.on('pageerror',e=>errors.push(e.message));
   await page.goto('http://127.0.0.1:4173/console.html?transport='+transport);
   await page.waitForFunction(()=>online&&libraryState&&wifiState&&!busy&&!polling);
   await page.evaluate(()=>showPage('settings'));await page.waitForFunction(()=>remoteState&&!remoteBusy);
   await page.evaluate(()=>{window.scanCalls=[];const original=libraryRequest;libraryRequest=(q,...args)=>{if(['wifi','remote'].includes(q.op))scanCalls.push({...q});return original(q,...args);};});
   for(const [op,open,dialog,close,refresh] of [['wifi','wifiScan','wifiScanDialog','wifiScanClose','wifiScanRefresh'],['remote','remoteScan','remotePairDialog','remotePairClose','remoteSearchAgain']]){
    // Close during the outstanding start, immediately reopen, then close via Escape.
    await page.evaluate(open=>$(open).click(),open);
    await page.waitForFunction(op=>scanCalls.some(q=>q.op===op&&q.action==='scan'),op);
    await page.evaluate(({open,close})=>{$(close).click();$(open).click();},{open,close});
    await page.waitForFunction(op=>scanCalls.some(q=>q.op===op&&q.action==='scanCancel'),op);
    await page.waitForFunction(op=>scanCalls.filter(q=>q.op===op&&q.action==='scan').length>=2,op);
    assert(!await page.locator('#'+open).evaluate(e=>e.classList.contains('is-loading')),'Card search never spins');
    assert(await page.locator('#'+open).isEnabled(),'Card search remains available');
    await page.keyboard.press('Escape');await page.waitForFunction(id=>!$(id).open,dialog);
    await page.waitForFunction(op=>scanCalls.filter(q=>q.op===op&&q.action==='scanCancel').length>=2,op);
    await page.locator('#'+open).click();
    await page.waitForFunction(({op,refresh})=>!$(refresh).disabled&&(op==='wifi'?wifiScanState?.scanState==='complete':!remoteState?.scanning),{op,refresh});
    await page.evaluate(()=>showPage('home'));
    await page.waitForFunction(id=>!$(id).open,dialog);
    await page.evaluate(()=>showPage('settings'));
    await page.waitForFunction(op=>scanCalls.filter(q=>q.op===op&&q.action==='scanCancel').length>=3,op);
   }
   const cancelled=await page.evaluate(()=>scanCalls.filter(q=>q.action==='scanCancel').map(q=>({op:q.op,owned:scanCalls.some(s=>s.op===q.op&&s.action==='scan'&&s.scanToken===q.scanToken)})));
   assert(cancelled.every(c=>c.owned),'Cancellation is tied to the originating scan');
   // Stale asynchronous scan starts may not resurrect closed dialogs or lock controls.
   await page.evaluate(()=>{const original=libraryRequest;libraryRequest=async(q,...args)=>{const value=await original(q,...args);if(q.action==='scan')await new Promise(r=>setTimeout(r,600));return value;};$('wifiScan').click();$('wifiScanClose').click();$('remoteScan').click();$('remotePairClose').click();});
   await page.waitForTimeout(1400);
   assert(await page.evaluate(()=>!wifiScanBusy&&!remoteBusy&&!$('wifiScanDialog').open&&!$('remotePairDialog').open));
   await page.evaluate(()=>{showPage('home');$('libraryList').style.minHeight='2400px';});
   await page.waitForTimeout(350);await page.evaluate(()=>window.scrollTo(0,1200));
   const home=await page.evaluate(()=>scrollY);assert(home>500);
   for(const target of ['settings','edit','play']){
    await page.evaluate(name=>showPage(name),target);await page.waitForTimeout(350);
    assert.equal(await page.evaluate(()=>scrollY),0,target+' starts at its own top');
    await page.evaluate(()=>showPage('home'));await page.waitForTimeout(350);
    assert(Math.abs(await page.evaluate(()=>scrollY)-home)<2,'Home position restored');
   }
   for(const [route,title] of [['play','currentTitle'],['edit','editorTitle']]){
    const info=await page.evaluate(({route,title})=>{showPage(route);$(title).textContent='超长文稿标题 Long manuscript title '.repeat(20);const el=$(title),s=getComputedStyle(el);return {nowrap:s.whiteSpace,overflow:s.overflow,ellipsis:s.textOverflow,height:el.offsetHeight,line:parseFloat(s.lineHeight),width:el.offsetWidth,scroll:el.scrollWidth};},{route,title});
    assert.equal(info.nowrap,'nowrap');assert.equal(info.ellipsis,'ellipsis');assert.equal(info.overflow,'hidden');assert(info.height<=info.line+2&&info.scroll>info.width);
   }
   const geometry=()=>page.evaluate(()=>{updateMobileViewport();return {shell:document.querySelector('.shell').offsetHeight,font:getComputedStyle($('editorTitle')).fontSize,top:getComputedStyle(document.querySelector('.top')).display};});
   await page.evaluate(()=>$('script').focus());const before=await geometry();
   await page.setViewportSize({width:390,height:480});await page.waitForTimeout(200);const after=await geometry();
   assert.equal(after.shell,before.shell,'Keyboard must not shrink editor shell');assert.equal(after.font,before.font);assert.equal(after.top,before.top,'Keyboard does not hide app header');
   await page.evaluate(()=>$('script').blur());await page.waitForTimeout(400);assert.equal((await geometry()).shell,before.shell,'Blur before keyboard dismissal also keeps the layout stable');
   await page.setViewportSize({width:390,height:844});
   await page.evaluate(()=>{showPage('settings');renderSdStatus({mounted:false,sdMounted:false,sdCanFormat:true,storageBackend:'internal',state:'unavailable',sdProbeError:'ESP_OK'});});
   assert.match(await page.locator('#sdRecovery').textContent(),/格式化.*清空/);
   assert(await page.locator('#sdFormat').isVisible());assert(await page.locator('#sdFormat').isEnabled());
   assert.match(await page.locator('#storageNotice').textContent(),/格式化/);
   await page.locator('#sdFormat').click();await page.waitForFunction(()=>$('confirmDialog').open);
   assert.match(await page.locator('#confirmText').textContent(),/整张 SD 卡/);await page.evaluate(()=>$('confirmDialog').close('cancel'));
   await page.waitForTimeout(50);
   await page.evaluate(()=>renderSdStatus({mounted:false,sdMounted:false,sdCanFormat:false,storageBackend:'internal',state:'unavailable',sdProbeError:'ESP_ERR_TIMEOUT'}));
   assert(await page.locator('#sdFormat').isHidden());assert.match(await page.locator('#sdRecovery').textContent(),/硬件读写故障/);
   await page.evaluate(()=>renderSdStatus({mounted:true,sdMounted:true,sdCanFormat:true,storageBackend:'internal',state:'ready'}));
   assert(await page.locator('#sdRecovery').isHidden());assert(await page.locator('#sdFormat').isHidden(),'Healthy standby SD is not an internal-library format action');
   assert.deepEqual(errors,[]);results.push({engine:engine.name(),transport,passed:true,cancelled});console.log('PASS',engine.name(),transport,'scan cancel/reopen/stale result/navigation/scroll/title/keyboard/SD advice');
   await context.close();
  }}finally{await browser.close();}
 }
 fs.writeFileSync('.tools/review22-21-browser-private.json',JSON.stringify({passed:true,results},null,2));
})().catch(e=>{console.error(e);process.exitCode=1;});
