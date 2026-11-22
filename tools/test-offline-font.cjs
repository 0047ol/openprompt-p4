const {chromium,webkit}=require('../.tools/browser-test/node_modules/playwright-core');
const fs=require('node:fs'),assert=require('node:assert/strict');
const manifest=JSON.parse(fs.readFileSync('web/reader-cdn.json','utf8'));
const packed=fs.readFileSync('firmware/p4/main/reader_shards.bin');
const shards=[...fs.readFileSync('firmware/p4/main/reader_shards.h','utf8').matchAll(/\{(\d+), (\d+),/g)].map(m=>packed.subarray(+m[1],+m[1]+ +m[2]));
const offline=Object.fromEntries(['sourceSha256','supplement','localOnly'].map(key=>[key,manifest[key]]));
offline.files=manifest.files.map(file=>({file:file.file,points:String.fromCodePoint(...file.points)}));
const source=fs.readFileSync('web/reader-font.js','utf8').replace('/* READER_CDN_SOURCE */',JSON.stringify('https://font.test/manifest.json')).replace('/* READER_OFFLINE_INDEX */',JSON.stringify(offline));
const mirrorOrigin=new URL(source.match(/const mirror='([^']+)'/)[1]).origin;
(async()=>{
 for(const engine of ['chromium','webkit']){
  const browser=await(engine==='webkit'?webkit:chromium).launch({headless:true,executablePath:engine==='webkit'?process.env.LOCALAPPDATA+'/ms-playwright/webkit-2370/Playwright.exe':'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
  try{
   for(const scenario of ['cdn','mirror','mirror-unavailable','offline','hotspot','slow-cdn','local-retry']){
    const context=await browser.newContext(),page=await context.newPage(),requests=[],local=[];
    const origin=['cdn','mirror','mirror-unavailable'].includes(scenario)?'https://controller.test':scenario==='hotspot'?'http://192.168.4.1':'http://192.0.2.10';
    let failLocal=scenario==='local-retry';
    await context.route(origin+'/**',route=>{
     const url=new URL(route.request().url());
     if(url.pathname==='/reader.woff2'){
      local.push(url.href);if(failLocal)return route.abort('failed');
      return route.fulfill({contentType:'font/woff2',body:shards[+url.searchParams.get('shard')]});
     }
     return route.fulfill({contentType:'text/html',body:'<!doctype html><html><body></body></html>'});
    });
    await context.route('https://**/*',async route=>{
     const url=route.request().url();if(url.startsWith(origin))return route.fallback();requests.push(url);
     if(['offline','hotspot','local-retry'].includes(scenario)||scenario==='mirror'&&url.includes('unpkg.com')||scenario==='mirror-unavailable'&&new URL(url).origin===mirrorOrigin)return route.abort('internetdisconnected');
     if(scenario==='slow-cdn'){await new Promise(r=>setTimeout(r,1800));return route.abort('timedout');}
     if(url.endsWith('manifest.json'))return route.fulfill({contentType:'application/json',headers:{'Access-Control-Allow-Origin':'*'},body:JSON.stringify(manifest)});
     return route.fulfill({contentType:'font/woff2',headers:{'Access-Control-Allow-Origin':'*'},body:fs.readFileSync('.tools/font-cdn/'+url.split('/').at(-1))});
    });
    await page.goto(origin);await page.addScriptTag({content:source});
    assert.equal(requests.length+local.length,0,'list never fetches fonts');
    const load=()=>page.evaluate(async()=>{try{await loadReaderTextFont('你好 Hello 123 螒');return {ok:true,files:readerFontFiles.size};}catch{return {ok:false};}});
    const start=Date.now();let result=await load();
    if(origin.startsWith('http:'))assert.deepEqual(await page.evaluate(()=>readerOfflineIndex.files.map(f=>f.points)),manifest.files.map(f=>f.points),'Compact offline index preserves every codepoint and shard order');
    if(failLocal){assert.equal(result.ok,false);failLocal=false;result=await load();}
    assert.equal(result.ok,true,scenario+' renders the actual device font');
    if(origin.startsWith('http:'))assert(local.length>0,'offline uses small device shards');
    else assert.equal(local.length,0,'public HTTPS keeps external font sources');
    if(scenario==='hotspot')assert.equal(requests.length,0,'hotspot never waits for internet');
    if(scenario==='slow-cdn')assert(Date.now()-start<4500,'bounded CDN wait then local fallback');
    const before=requests.length+local.length;await load();assert.equal(requests.length+local.length,before,'font shards reused');
    console.log('PASS',engine,scenario,result,'local shards:',local.length);await context.close();
   }
  }finally{await browser.close();}
 }
})().catch(e=>{console.error(e);process.exitCode=1;});
