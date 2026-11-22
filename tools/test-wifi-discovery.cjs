/* User-visible discovery, secure/open connection and real asynchronous phases.
 * Mock radio responses validate browser behavior, not physical authentication. */
const {chromium,webkit}=require('../.tools/browser-test/node_modules/playwright-core');
const assert=require('node:assert/strict'),fs=require('node:fs');
(async()=>{
 fs.mkdirSync('.tools/review22-17',{recursive:true});
 for(const engine of [chromium,webkit]){
  const browser=await engine.launch({headless:true,executablePath:engine===webkit?process.env.LOCALAPPDATA+'/ms-playwright/webkit-2370/Playwright.exe':'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
  try{for(const transport of ['lan','ble']){
   const context=await browser.newContext({locale:'zh-CN',viewport:{width:1366,height:1000}}),page=await context.newPage(),errors=[];
   page.on('pageerror',e=>errors.push(e.message));
   await page.goto('http://127.0.0.1:4173/console.html?transport='+transport+'&view=settings');
   await page.waitForFunction(()=>online&&wifiState?.scanSupported&&!busy&&!polling);
   await page.evaluate(()=>{
    const original=wifiRequest;window.scanWrites=[];window.wifiFail=false;
    wifiRequest=async(action='status',values={})=>{
     if(action!=='save'||!('ssid' in values))return original(action,values);
     scanWrites.push({ssid:values.ssid,password:values.password});
     const result=await original(action,values);
     TP_DEMO.setWifi({connected:false,status:'connecting',phase:'connecting',switching:false,ip:''});
     setTimeout(()=>TP_DEMO.setWifi({phase:'dhcp'}),1200);
     // The test controls the fake network's completion after observing DHCP.
     // Serialized BLE requests must not race a short wall-clock phase timer.
     window.completeWifiChoice=()=>TP_DEMO.setWifi(wifiFail?{status:'failed',phase:'failed'}:{connected:true,status:'connected',phase:'connected',ip:'192.0.2.10'});
     return {...result,connected:false,status:'connecting',phase:'connecting',switching:false};
    };
   });
   const openSearch=async()=>{await page.locator('#wifiScan').click();await page.waitForFunction(()=>wifiScanState?.scanState==='complete'&&$('wifiScanList').children.length===3);};
   await openSearch();
   assert.match(await page.locator('#wifiScanList').textContent(),/WPA2/);
   assert.match(await page.locator('#wifiScanList').textContent(),/-48 dBm/);
   assert(await page.locator('#wifiScanList .remote-device').filter({hasText:'Enterprise Demo'}).locator('button').isDisabled());
   for(const language of ['zh-CN','en'])for(const width of [1366,390,320])for(const theme of ['dark','light']){
    await page.setViewportSize({width,height:900});await page.evaluate(({language,theme})=>{applyLanguage(language);document.documentElement.dataset.theme=theme;renderWifiScan();},{language,theme});
    const geometry=await page.locator('#wifiScanDialog').evaluate(el=>{const b=el.getBoundingClientRect();return {left:b.left,right:b.right,scroll:el.scrollWidth,width:el.clientWidth,refresh:document.querySelector('#wifiScanRefresh').getBoundingClientRect().toJSON(),heading:el.querySelector('h3').getBoundingClientRect().toJSON(),rows:[...el.querySelectorAll('.remote-device')].map(n=>n.getBoundingClientRect().toJSON())};});
    assert(geometry.left>=0&&geometry.right<=width+1&&geometry.scroll<=geometry.width+1,JSON.stringify(geometry));
    assert(geometry.refresh.left>geometry.heading.right,'Refresh stays at the right of title');
    for(const row of geometry.rows)assert(row.right<=geometry.right&&row.left>=geometry.left,'All rows fit');
    if(engine===chromium&&transport==='lan'&&width===390&&theme==='dark')await page.locator('#wifiScanDialog').screenshot({path:'.tools/review22-17/wifi-search-'+language+'.png'});
   }
   await page.evaluate(()=>applyLanguage('zh-CN'));await page.setViewportSize({width:390,height:900});
   await page.locator('#wifiScanList .remote-device').filter({hasText:'Sample Network'}).locator('button').click();
   assert(await page.locator('#wifiScanPassword').isVisible());assert.equal(await page.evaluate(()=>scanWrites.length),0,'Encrypted network waits for a password');
   await page.locator('#wifiScanPassword').fill('short');await page.locator('#wifiScanConnect').click();assert.equal(await page.evaluate(()=>scanWrites.length),0,'Invalid password is not sent');
   await page.locator('#wifiScanPassword').fill('demo-test-password');await page.locator('#wifiScanConnect').click();
   await page.waitForFunction(()=>wifiJoin&&$('wifiScanStatus').textContent.includes('连接'));
   assert(await page.locator('#wifiScanDialog').evaluate(el=>el.open),'Submitted credentials alone must not close');
   await page.waitForFunction(()=>scanWrites.length===1&&!wifiPending&&typeof completeWifiChoice==='function');
   await page.evaluate(()=>{++epoch;renderWifiScan();});
   assert(await page.evaluate(()=>!!wifiJoin),'A brief same-device reconnect retains connection verification');
   await page.waitForFunction(()=>$('wifiScanStatus').textContent.includes('获取网络地址'));
   assert(await page.locator('#wifiScanDialog').evaluate(el=>el.open),'Getting IP is not completed');
   await page.evaluate(()=>completeWifiChoice());
   await page.waitForFunction(()=>!wifiJoin&&!$('wifiScanDialog').open);
   assert.equal(await page.locator('#wifiScanPassword').inputValue(),'','Password cleared after successful connection');
   assert.equal(await page.evaluate(()=>scanWrites.length),1,'Only one credential write');
   assert.equal(await page.evaluate(()=>pageName),'settings','No forced navigation or reboot');
   await openSearch();await page.locator('#wifiScanList .remote-device').filter({hasText:'Open Demo Network'}).locator('button').click();
   await page.waitForFunction(()=>scanWrites.length===2);assert.deepEqual(await page.evaluate(()=>scanWrites[1]),{ssid:'Open Demo Network',password:''});
   assert.equal(await page.locator('#wifiScanPassword').isVisible(),false,'Open network does not prompt for a password');
   await page.waitForFunction(()=>$('wifiScanStatus').textContent.includes('获取网络地址'));await page.evaluate(()=>completeWifiChoice());
   await page.waitForFunction(()=>!wifiJoin&&!$('wifiScanDialog').open);
   await page.evaluate(()=>wifiFail=true);await openSearch();await page.locator('#wifiScanList .remote-device').filter({hasText:'Sample Network'}).locator('button').click();
   await page.locator('#wifiScanPassword').fill('wrong-demo-password');await page.locator('#wifiScanConnect').click();
   await page.waitForFunction(()=>$('wifiScanStatus').textContent.includes('获取网络地址'));await page.evaluate(()=>completeWifiChoice());
   await page.waitForFunction(()=>!wifiJoin&&wifiScanState?.scanState==='connectError');assert(await page.locator('#wifiScanDialog').evaluate(el=>el.open),'Failure stays open for correction');
   assert.match(await page.locator('#wifiScanStatus').textContent(),/连接失败/);assert(await page.locator('#wifiScanConnect').isEnabled());
   await page.locator('#wifiScanClose').click();assert.equal(await page.locator('#wifiScanPassword').inputValue(),'','Close clears secret');
   await page.evaluate(()=>{online=false;renderWifiScan();});assert(await page.locator('#wifiScan').isDisabled());
   assert.deepEqual(errors,[]);console.log('PASS',engine.name(),transport,'scan/security/RSSI/open vs encrypted/validation/connecting-DHCP/success close/failure retry/no navigation/320px bilingual theme/offline');
   await context.close();
  }}finally{await browser.close();}
 }
})().catch(error=>{console.error(error);process.exitCode=1;});
