/* Paragraph boundaries and affected UI, using the shared virtual LAN/GATT. */
const {chromium,webkit}=require('../.tools/browser-test/node_modules/playwright-core');
const assert=require('node:assert/strict'),fs=require('node:fs');
(async()=>{
 fs.mkdirSync('.tools/review22-14',{recursive:true});
 for(const engine of [chromium,webkit]){
  const browser=await engine.launch({headless:true,executablePath:engine===webkit?process.env.LOCALAPPDATA+'/ms-playwright/webkit-2370/Playwright.exe':'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
  try{for(const mode of ['lan','ble']){
   const page=await browser.newPage({locale:'zh-CN',viewport:{width:1366,height:1000}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
   await page.goto('http://127.0.0.1:4173/console.html?view=play&transport='+mode);
   await page.waitForFunction(()=>online&&previewLayout&&!busy&&!polling&&!playbackDocumentPending());
   const result=await page.evaluate(async()=>{
    poll=async()=>{};await queue;
    const check=(v,label)=>{if(!v)throw Error(label);};
    const settle=async()=>{for(let i=0;i<100;i++){await refreshPreviewLayout();render(await readStatus());if(previewMatchesDocument(previewLayout)&&(!lastState.pageMode||lastState.pageCount))return;await new Promise(r=>setTimeout(r,40));}throw Error('layout timeout');};
    let id=0;
    const open=async text=>{const title='Paragraph regression '+(++id);check((await libraryRequest({op:'create',title,text,style:{pageMode:1,pageParagraphs:1,font:60,lineSpacing:20,countdown:0,pageSeconds:1}})).ok,'create');await openPlayback(title);await queue;await settle();};
    await open('First line\nSecond line\n\nNext paragraph.');
    check(lastState.pageCount===2,'blank line starts next page');check(previewLayout.pages[0][1]===2,'single newline stays on same page');
    await applyControl({pageStep:1});render(await readStatus());check(readingPage(previewLayout,lastState.pageIndex)[0]===3,'manual next uses exact variable start');
    check(previewMotion.position(performance.now())===readingPageOffset(previewLayout,1),'preview uses paragraph page offset');
    await control({rotation:0});await settle();check(lastState.pageIndex===1&&previewLayout.rows[readingPage(previewLayout,1)[0]].text.startsWith('Next'),'reflow retains paragraph anchor');
    await control({pageParagraphs:0});await settle();check(lastState.pageCount===Math.ceil(previewLayout.playbackRows/previewLayout.pageRows),'off restores continuous pages');
    check((await libraryRequest({op:'get',title:'Paragraph regression 1'})).text==='First line\nSecond line\n\nNext paragraph.','original blank lines preserved');
    await control({pageParagraphs:1});await settle();check(lastState.pageCount===2,'on restores paragraph boundary');
    await open('\n\nA\n\n \t\n\nB\n\n');check(lastState.pageCount===2&&previewLayout.pages.every(p=>p[1]===1),'consecutive/space-only/leading/trailing blank lines make no empty page');
    await open('A\r\nB\r\n\r\nC');check(lastState.pageCount===2,'Windows CRLF paragraph');
    await open(Array.from({length:22},(_,i)=>'Line '+i).join('\n')+'\n\nTAIL');
    check(lastState.pageCount===Math.ceil(22/previewLayout.pageRows)+1,'long paragraph uses capacity then next paragraph');
    for(const span of previewLayout.pages)check(span[1]<=previewLayout.pageRows,'complete rows fit page');
    for(const bad of [-1,2]){let rejected=false;try{await applyControl({pageParagraphs:bad});}catch{rejected=true;}check(rejected,'invalid toggle rejected');}
    check(!$('pageParagraphsControl').hidden&&$('pageParagraphs').checked,'page-only toggle visible');
    await control({pageMode:0});await settle();check($('pageParagraphsControl').hidden,'scroll mode hides paragraph switch');
    return {cases:id,pages:previewLayout.pages.length};
   });
   await page.locator('#brightnessShortcut').click();await page.waitForFunction(()=>$('brightnessPopover').matches(':popover-open'));
   await page.locator('#brightnessNumber').fill('83');await page.locator('#brightnessNumber').press('Tab');await page.waitForFunction(()=>lastState.brightness===83&&!busy);
   await page.locator('#brightnessShortcut').click();
   for(const [i,title] of [[0,'左对齐'],[1,'居中'],[2,'右对齐']]){
    const button=page.locator('[data-align="'+i+'"]');assert((await button.boundingBox()).height>=44);await button.hover();await page.waitForFunction(()=>!$('tooltip').hidden);assert.equal(await page.locator('#tooltip').textContent(),title);await page.mouse.move(0,0);
   }
   await page.evaluate(()=>message('稿件已删除'));await page.waitForTimeout(250);
   assert((await page.locator('#message').boundingBox()).width<220,'short toast fits text');
   for(const width of [1366,390,320]){
    await page.setViewportSize({width,height:900});await page.evaluate(()=>showPage('settings'));await page.waitForTimeout(250);
    const gap=await page.evaluate(()=>{const a=$('multiControl').closest('label').getBoundingClientRect(),b=document.querySelector('.bar-visibility').getBoundingClientRect(),c=$('showHeader').closest('label').getBoundingClientRect();return {above:b.top-a.bottom,below:c.top-b.top-1};});
    assert(Math.abs(gap.above-gap.below)<1,'divider symmetric '+JSON.stringify(gap));
    await page.evaluate(()=>{showPage('home');$('libraryList').style.minHeight='1500px';window.scrollTo(0,0);});await page.waitForTimeout(200);
    const initial=await page.locator('.top .mark').boundingBox();await page.evaluate(()=>window.scrollTo(0,500));await page.waitForTimeout(250);const scrolled=await page.locator('.top .mark').boundingBox();
    assert(Math.abs(initial.y-scrolled.y)<1,'header inset retained after scroll '+JSON.stringify({width,initial:initial.y,scrolled:scrolled.y}));
    assert(initial.y>=20,'header has outer whitespace');
    await page.evaluate(width=>message('已保存 '+width),width);await page.waitForTimeout(200);assert((await page.locator('#message').boundingBox()).width<220,'mobile toast fits text');
    await page.screenshot({path:'.tools/review22-14/'+engine.name()+'-'+mode+'-'+width+'-sticky.png'});
   }
   assert.deepEqual(errors,[]);console.log('PASS',engine.name(),mode,JSON.stringify(result),'paragraph pages, anchor, toggle, brightness popover, align tooltips, divider, adaptive toast, sticky inset');await page.close();
  }}finally{await browser.close();}
 }
})().catch(error=>{console.error(error);process.exitCode=1;});
