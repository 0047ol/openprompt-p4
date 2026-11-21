/* Notification routing and the three reported UI defects, using shared pages. */
const {chromium,webkit}=require('../.tools/browser-test/node_modules/playwright-core');
const assert=require('node:assert/strict'),fs=require('node:fs');
const base=process.env.TP_DEMO_BASE||'http://127.0.0.1:4173';
const output=process.env.TP_UI_EVIDENCE||'logs/notification-policy';
(async()=>{
 fs.mkdirSync(output,{recursive:true});
 for(const engine of [chromium,webkit]){
  const browser=await engine.launch({headless:true,executablePath:engine===webkit?process.env.LOCALAPPDATA+'/ms-playwright/webkit-2370/Playwright.exe':'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
  try{
   const context=await browser.newContext({locale:'zh-CN',reducedMotion:'reduce',viewport:{width:1366,height:1000}});
   await context.addInitScript(()=>{const native=window.setInterval;window.policyPollTimers=[];window.setInterval=(fn,delay,...args)=>{const timer=native(fn,delay,...args);if(delay===500)window.policyPollTimers.push(timer);return timer;};});
   const page=await context.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));
   await page.goto(base+'/console.html');await page.waitForFunction(()=>online&&deviceSettings&&libraryState?.items?.length);
   await page.evaluate(()=>{window.policyPollTimers.forEach(clearInterval);clearTimeout(liveSocketRetry);if(liveSocket){liveSocket.onclose=null;liveSocket.close();liveSocket=null;}});
   for(const width of [320,390,650,1366])for(const theme of ['light','dark'])for(const language of ['zh-CN','en']){
    await page.setViewportSize({width,height:1000});
    await page.evaluate(({theme,language})=>{document.documentElement.dataset.theme=theme;applyLanguage(language);renderCrashRecovery();showPage('settings');}, {theme,language});
    const fields=await page.evaluate(()=>[...document.querySelectorAll('label.firmware-automatic')].map(label=>{
     const box=el=>el.getBoundingClientRect().toJSON();return {icons:label.querySelectorAll('.info-button').length,heading:box(label.querySelector('.field-heading')),icon:box(label.querySelector('.info-button')),input:box(label.querySelector('input'))};
    }));
    for(const field of fields){assert.equal(field.icons,1);assert(field.icon.right<field.input.left,'Info icon never overlays the switch');assert(field.icon.left>=field.heading.left);}
    const developer=await page.evaluate(()=>({body:$('diagnosticsCard').getBoundingClientRect().width,summary:$('diagnosticsSummary').getBoundingClientRect().width,display:getComputedStyle($('developerSettings').querySelector('.wifi-body')).display}));
    assert.equal(developer.display,'block');assert(Math.abs(developer.body-developer.summary)<1,'Diagnostic summary uses full card width');
    await page.evaluate(()=>showPage('play'));
    for(const id of ['brightness','textColor','headerHeight']){
     const feedback=await page.evaluate(id=>{
      const el=$(id);el.disabled=false;el.removeAttribute('data-pending');const active=getComputedStyle(el).opacity;
      setControlDisabled(el,true);const disabled={opacity:getComputedStyle(el).opacity,cursor:getComputedStyle(el).cursor};
      setControlDisabled(el,false,true);const pending=getComputedStyle(el).opacity;setControlDisabled(el,false);
      return {active,disabled,pending};
     },id);
     assert(Number(feedback.disabled.opacity)<Number(feedback.active),'Unavailable controls are visibly dimmed: '+id);
     assert.equal(feedback.disabled.cursor,'not-allowed');assert.equal(feedback.pending,feedback.active,'Temporary request locks do not flicker');
    }
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'No page overflow');
   }
   await page.evaluate(()=>{applyLanguage('zh-CN');showPage('home');firmwareState={version:'20261122.2',phase:'idle'};lifecycleIntent=null;lastState={...lastState,lifecycle:'ready'};renderDeviceActivity();});
   const homeTop=await page.locator('#homePage').evaluate(el=>el.getBoundingClientRect().top);
   for(const lifecycle of ['powering_off','powering_on','restarting','starting','resetting']){
    await page.evaluate(lifecycle=>{lifecycleIntent=null;online=true;deviceResponseAt=performance.now();lastState={...lastState,lifecycle,powerProgress:43};firmwareState={...firmwareState,phase:'idle'};renderDeviceActivity();},lifecycle);
    assert(await page.locator('#toastStack #activityNotice').isVisible(),lifecycle+' is a Toast');
    assert(await page.locator('#devicePowerNotice').isHidden(),lifecycle+' has no power banner');
    assert.equal(await page.locator('#homePage').evaluate(el=>el.getBoundingClientRect().top),homeTop,'Progress never moves page content');
   }
   for(const phase of ['downloading','uploading','checking','restarting','resetting']){
    await page.evaluate(phase=>{lastState={...lastState,lifecycle:'ready'};lifecycleIntent=null;firmwareState={...firmwareState,phase,size:100,received:42};renderDeviceActivity();},phase);
    assert(await page.locator('#toastStack #activityNotice').isVisible());
    assert.equal(await page.locator('#noticeStack #activityNotice').count(),0);
   }
   await page.evaluate(()=>{firmwareState={...firmwareState,phase:'idle'};lastState={...lastState,lifecycle:'powered_off'};renderDeviceActivity();});
   assert(await page.locator('#devicePowerNotice').isVisible());assert(await page.locator('#activityNotice').isHidden());
   assert.equal(await page.locator('#noticeStack strong,#noticeStack h3').count(),0,'Banners have no extra titles');
   await page.evaluate(()=>{lastState={...lastState,lifecycle:'ready'};renderDeviceActivity();message('操作完成');});
   assert(await page.locator('#toastStack #message').isVisible());
   await page.waitForTimeout(3200);assert(await page.locator('#message').isHidden(),'Action feedback expires');
   await page.evaluate(()=>{const el=document.createElement('div');el.id='policyToast';el.hidden=true;$('toastStack').append(el);statusToast(el,'test-expiration',{duration:50});});
   await page.waitForTimeout(100);
   await page.evaluate(()=>{statusToast($('policyToast'),'test-expiration',{duration:50});});
   assert(await page.locator('#policyToast').isHidden(),'Polling never revives an expired Toast');
   await page.locator('#policyToast').evaluate(el=>el.remove());
   await page.evaluate(()=>{documentConflict=true;showPage('edit');});
   assert(await page.locator('#editorConflictNotice').isVisible());
   await page.evaluate(()=>{showPage('home');documentConflict=false;});
   assert(await page.locator('#editorConflictNotice').isHidden());
   await page.setViewportSize({width:650,height:1000});
   await page.evaluate(()=>{document.documentElement.dataset.theme='light';showPage('settings');});
   await page.locator('#firmwareSettings').screenshot({path:output+'/'+engine.name()+'-firmware-light.png'});
   await page.locator('#developerSettings').screenshot({path:output+'/'+engine.name()+'-developer-light.png'});
   await page.evaluate(()=>{showPage('play');setPlaybackSheet(true,{animate:false});$('brightness').disabled=true;$('textColor').disabled=true;});
   await page.locator('#playPage .playback-settings').screenshot({path:output+'/'+engine.name()+'-disabled-light.png'});
   assert.deepEqual(errors,[]);
   console.log('PASS',engine.name(),'32 viewport/theme/language combinations; help/switch separation, full-width diagnostic text, disabled controls, all transient lifecycle routes, persistent power/conflict banners, toast expiry and poll deduplication');
   await context.close();
  }finally{await browser.close();}
 }
})().catch(error=>{console.error(error);process.exitCode=1;});
