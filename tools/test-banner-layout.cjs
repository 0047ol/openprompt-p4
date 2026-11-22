/* Shared banner templates: real DOM/CSS, synthetic messages; no device calls. */
const {chromium,webkit}=require('../.tools/browser-test/node_modules/playwright-core');
const assert=require('node:assert/strict'),fs=require('node:fs');
(async()=>{
 fs.mkdirSync('.tools/review22-14',{recursive:true});
 for(const engine of [chromium,webkit]){
  const browser=await engine.launch({headless:true,executablePath:engine===webkit?process.env.LOCALAPPDATA+'/ms-playwright/webkit-2370/Playwright.exe':'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
  try{
   const page=await browser.newPage({locale:'zh-CN',reducedMotion:'reduce',viewport:{width:1366,height:1000}}),errors=[];
   page.on('pageerror',e=>errors.push(e.message));
   await page.addInitScript(()=>{window.layoutIntervals=[];const start=window.setInterval;window.setInterval=(...args)=>{const id=start(...args);window.layoutIntervals.push(id);return id;};});
   await page.goto('http://127.0.0.1:4173/console.html?transport=ble');
   await page.waitForFunction(()=>online&&!busy&&!polling);
   await page.evaluate(async()=>{poll=async()=>{};await queue;for(const id of layoutIntervals)clearInterval(id);if(liveSocket){liveSocket.close();liveSocket=null;}});
   await page.waitForTimeout(500);
   for(const width of [1366,390,320])for(const theme of ['light','dark']){
    await page.setViewportSize({width,height:1200});
    await page.evaluate(theme=>{
     document.documentElement.dataset.theme=theme;
     showPage('settings');
     for(const el of document.querySelectorAll('#noticeStack>.connect-panel,.network-reconnect')){
      el.hidden=false;el.querySelector('p').textContent='设备状态已更新';
      for(const button of el.querySelectorAll('button'))button.hidden=false;
      const link=el.querySelector('a');if(link)link.textContent='打开后台';
     }
    },theme);
    await page.waitForTimeout(150);
    const measure=()=>page.evaluate(()=>[...document.querySelectorAll('#noticeStack>.connect-panel,.network-reconnect')].map(el=>{
     const r=el.getBoundingClientRect(),s=getComputedStyle(el),icon=getComputedStyle(el,'::before');
     const action=el.querySelector('button:not([hidden]),.activity-action,a'),a=action?.getBoundingClientRect();
     return {id:el.id,h:r.height,padding:s.padding,icon:icon.width,center:a?Math.abs(a.y+a.height/2-r.y-r.height/2):0,action:a?.height,overflow:el.scrollWidth>el.clientWidth};
    }));
    let rows=await measure();
    for(const row of rows){assert.equal(row.h,48,JSON.stringify({width,theme,...row}));assert.equal(row.padding,'7px 12px');assert.equal(row.icon,'18px');assert(row.center<.1);assert(!row.overflow);if(row.action)assert.equal(row.action,32);}
    await page.evaluate(()=>$('devicePowerNoticeStart').hidden=true);
    assert.equal(await page.locator('#devicePowerNotice').evaluate(el=>getComputedStyle(el).gridTemplateColumns.split(' ').length),2,'hidden action consumes no extra column');
    await page.evaluate(()=>{
     $('activitySpinner').hidden=true;$('activityMeter').hidden=false;$('activityPercent').hidden=false;$('activityPercent').textContent='100%';$('activityRing').style.strokeDashoffset='0';
     for(const el of document.querySelectorAll('#noticeStack>.connect-panel,.network-reconnect'))el.querySelector('p').textContent='设备暂时无法连接，请检查设备电源与网络状态，然后重新连接。';
     $('activityTitle').textContent='正在写入固件，请保持设备供电并等待更新完成。';
    });
    rows=await measure();
    for(const row of rows){assert(row.h>=48);assert(row.center<.1,'multiline action stays centered '+JSON.stringify(row));assert(!row.overflow);}
    const ring=await page.locator('#activityMeter').boundingBox();assert.equal(ring.width,32);assert.equal(ring.height,32);
    assert(await page.locator('#activityPercent').isVisible());assert.equal(await page.locator('#activityPercent').textContent(),'100%');
    await page.screenshot({path:'.tools/review22-14/banners-'+engine.name()+'-'+width+'-'+theme+'.png',fullPage:true});
    console.log('PASS',engine.name(),width,theme,rows.length,'banner templates: 48px single line, shared padding/icon/action, multiline centered, progress fits');
   }
   assert.deepEqual(errors,[]);await page.close();
  }finally{await browser.close();}
 }
})().catch(error=>{console.error(error);process.exitCode=1;});
