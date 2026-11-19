// Screen chrome preferences are device settings shared by both transports.
const screenBarDefaults=/* SCREEN_BAR_DEFAULTS */;
const barOptions=[['时间',1],['日期',2],['设备名称',131072],['播放状态',4],['上一篇稿件',8388608],['下一篇稿件',8],['循环次数',2048],['播放倍率',16],['帧率',32],['缓冲',128],['帧间抖动',256],['绘制耗时',512],['播放进度',1024],['内存',4096],['字号',16384],['屏幕亮度',32768],['字间距',262144],['行间距',524288],['左右边距',1048576],['倒计时',2097152],['像素码率',4194304],['绘制帧数',16777216],['延迟帧数',33554432]];
barOptions.splice(3,0,['播放时间码',67108864]);
const headerBits=new Set([1,2,4,8,2048,131072,8388608,67108864]);
var screenTelemetry=null;
function playbackBars(state=lastState,settings=deviceSettings||{}){
  const value=state?.showInfo;
  if(value===0||value===1)return {header:!!value,footer:!!value};
  return {header:settings.showHeader??settings.showInfo??true,footer:settings.showFooter??settings.showInfo??true};
}
function renderPlaybackBarVisibility(){
  if(editing('showInfo'))return;
  const bars=playbackBars();$('showInfo').checked=bars.header||bars.footer;
}
let telemetryReadAt=0,telemetryReading=false,telemetryEpoch=-1;
const barsSection=document.createElement('section');barsSection.className='wifi';barsSection.id='screenBarsSettings';
barsSection.innerHTML='<h3 class="settings-title">标题栏与状态栏</h3><div class="wifi-body"><div class="bar-visibility"><label class="check switch"><input id="showHeader" type="checkbox" role="switch" data-device disabled>显示标题栏</label><label class="check switch"><input id="showFooter" type="checkbox" role="switch" data-device disabled>显示状态栏</label></div><div class="bar-heights"></div><fieldset class="bar-fields" data-bar-group="header"><legend>标题栏内容</legend></fieldset><fieldset class="bar-fields" data-bar-group="footer"><legend>状态栏内容</legend></fieldset><p id="barCapacity" class="context-status" aria-live="polite"></p><p class="context-status">标题居中；左侧日期时间，右侧播放状态和稿件信息。循环时显示次数，自动播放下一篇时显示下一篇。</p></div>';
const sharedBarControls=[['headerHeight','footerHeight','标题/状态栏高度',screenBarDefaults.headerHeight,24,160,4],['headerFontSize','footerFontSize','标题/状态栏字号',screenBarDefaults.headerFontSize,12,40,1]];
const headerExplanation=barsSection.querySelector('.wifi-body').lastElementChild;
const headerHelp=document.createElement('button');headerHelp.type='button';headerHelp.className='info-button';headerHelp.setAttribute('aria-label',tr('说明'));headerHelp.dataset.tooltip=headerExplanation.textContent;
barsSection.querySelector('[data-bar-group=header] legend').append(headerHelp);headerExplanation.remove();
const capacityNote=barsSection.querySelector('#barCapacity');capacityNote.hidden=true;
const capacityHelp=document.createElement('button');capacityHelp.id='barCapacityInfo';capacityHelp.type='button';capacityHelp.className='info-button';capacityHelp.setAttribute('aria-label',tr('说明'));capacityHelp.dataset.tooltip='最多选择 6 项。';
barsSection.querySelector('[data-bar-group=footer] legend').append(capacityHelp);
for(const [id,,label,initial,min,max,step] of sharedBarControls){
  barsSection.querySelector('.bar-heights').insertAdjacentHTML('beforeend',`<div class="bar-height-control"><label for="${id}"><span class="bar-height-name">${label}</span><span class="bar-height-value"><output id="${id}Value">${initial}</output> px</span></label><div class="numeric-control"><input id="${id}" type="range" min="${min}" max="${max}" step="${step}" value="${initial}" data-device disabled><input id="${id}Number" type="number" aria-label="${label}" min="${min}" max="${max}" step="${step}" value="${initial}" data-device disabled></div></div>`);
}
$('systemSettings').after(barsSection);
for(const [label,bit] of barOptions){const el=document.createElement('label');el.className='check';el.innerHTML=`<input type="checkbox" data-bar-bit="${bit}" data-device disabled><span>${label}</span>`;barsSection.querySelector(`[data-bar-group="${headerBits.has(bit)?'header':'footer'}"]`).append(el);el.querySelector('input').onchange=()=>{const mask=[...barsSection.querySelectorAll('[data-bar-bit]:checked')].reduce((sum,input)=>sum+Number(input.dataset.barBit),0);renderBarCapacity();void saveImmediateSetting({barFields:mask});};}
for(const id of ['showHeader','showFooter'])$(id).onchange=()=>saveImmediateSetting({[id]:$(id).checked});
for(const [id,paired] of sharedBarControls){
  $(id).oninput=()=>{$(id+'Value').value=$(id).value;$(id+'Number').value=$(id).value;updateRangeAppearance($(id));};
  $(id).onchange=()=>{const value=Number($(id).value);return saveImmediateSetting({[id]:value,[paired]:value});};
  $(id+'Number').onchange=()=>{const input=$(id+'Number');if(!input.value||!input.checkValidity()){input.value=$(id).value;return;}$(id).value=input.value;$(id).dispatchEvent(new Event('input'));$(id).dispatchEvent(new Event('change'));};
}
const barTextWalker=document.createTreeWalker(barsSection,NodeFilter.SHOW_TEXT);
while(barTextWalker.nextNode()){const node=barTextWalker.currentNode,source=node.textContent,trimmed=source.trim();if(translations[trimmed])localizedNodes.push({node,source,trimmed});}
applyLanguage(uiLanguage);
const renderSettingsWithBars=renderDeviceSettings;
function screenBarButtons(){
  const section=$('screenBarsSettings');if(!section)return;
  for(const input of section.querySelectorAll('input')){
    const key=input.id.replace(/Number$/,'');
    const unsupported=key.endsWith('FontSize')&&deviceSettings?.[key]===undefined;
    const newField=(Number(input.dataset.barBit)>=65536&&deviceSettings?.headerFontSize===undefined)||(Number(input.dataset.barBit)===67108864&&!deviceSettings?.timecodeSupported);
    setControlDisabled(input,!online||firmwareLocked()||deviceSettings?.barFields===undefined||unsupported||newField,busy||playbackPending||libraryActionPending);
  }
  renderBarCapacity();
}
renderDeviceSettings=function(settings){
  renderSettingsWithBars(settings);
  screenBarButtons();
  for(const id of ['showHeader','showFooter'])$(id).checked=settings[id]??settings.showInfo??true;
  for(const [id] of sharedBarControls){if(editing(id)||editing(id+'Number')||document.activeElement===$(id+'Number'))continue;if(id.endsWith('Height'))$(id).min=settings.headerFontSize===undefined?48:24;$(id+'Number').min=$(id).min;$(id).value=settings[id]??screenBarDefaults[id];$(id+'Number').value=$(id).value;$(id+'Value').value=$(id).value;updateRangeAppearance($(id));}
  for(const input of barsSection.querySelectorAll('[data-bar-bit]'))input.checked=!!((settings.barFields??screenBarDefaults.barFields)&Number(input.dataset.barBit));
  renderBarCapacity();
};
async function refreshScreenTelemetry(){
  if(telemetryEpoch!==epoch){telemetryEpoch=epoch;screenTelemetry=null;telemetryReadAt=0;}
  if(pageName!=='play'||!online||busy||telemetryReading||firmwareLocked()||performance.now()-telemetryReadAt<2000)return;
  if(deviceSettings?.barFields===undefined)return; // Older firmware has no telemetry RPC.
  telemetryReadAt=performance.now();telemetryReading=true;const version=epoch;
  try{const value=await libraryRequest({op:'telemetry'});if(value.ok&&version===epoch)screenTelemetry=value;}catch{}finally{telemetryReading=false;}
}
// Tokens reserve five spaces; icons never depend on font symbol coverage.
function screenBarVisible(text){return String(text).replaceAll('|',' ').replace(/\{(?:sun|clk)\}/g,'     ');}
const screenBarPaths=[
  [[10,1,10,3],[10,17,10,19],[1,10,3,10],[17,10,19,10],[4,4,5,5],[15,15,16,16],[4,16,5,15],[15,5,16,4]],
  [[10,5,10,10],[10,10,14,12]]
];
function screenBarFieldMetrics(field,ctx){
  const match=/^\{(sun|clk)\} /.exec(field),font=parseFloat(ctx.font)||22;
  const iconSize=Math.max(8,Math.floor(font*3/4)),text=match?field.slice(6):field;
  return {text,type:match?match[1]:null,iconSize,width:ctx.measureText(text).width+(match?iconSize+6:0)};
}
function screenBarFit(fields,width,ctx){
  for(let count=Math.min(fields.length,6);count>=0;--count){
    const text=fields.slice(0,count).join(' | ');
    const measured=fields.slice(0,count).reduce((sum,field,i)=>sum+screenBarFieldMetrics(field,ctx).width+(i?24:0),0);
    if(measured<=width||!count)return {text,shown:count,total:fields.length,width:measured};
  }
}
function screenBarFields(state,mask){
  const t=screenTelemetry,en=uiLanguage==='en',part=(bit,zh,eng,value)=>mask&bit?(en?eng:zh)+(value??''):null;
  const memory=t?.totalMemory?`${(t.freeMemory/1048576).toFixed(1)}/${(t.totalMemory/1048576).toFixed(1)} MB`:'—';
  return [part(16,'','',`x${state.shuttle||1}`),part(32,'','',`${t?.fps?.toFixed(1)??'—'} fps`),part(128,'缓冲 ','Buffer ',t?`${t.bufferReady}/${t.bufferTotal}`:'—'),part(256,'抖动 ','Jitter ',`${t?.jitterMs?.toFixed(1)??'—'} ms`),part(512,'绘制 ','Render ',`${t?.renderMs?.toFixed(1)??'—'} ms`),part(1024,'','',`${Math.min(100,Math.round((state.scroll||0)/Math.max(1,previewLayout?.endScroll||1)*100))}%`),part(4096,'RAM ','RAM ',memory),part(16384,'Aa ','Aa ',`${state.font} px`),part(32768,'{sun} ','{sun} ',`${state.brightness}%`),part(262144,'字距 ','Letter ',`${state.letterSpacing} px`),part(524288,'行距 ','Line ',`${state.lineSpacing} px`),part(1048576,'边距 ','Margins ',`${state.marginLeft}/${state.marginRight} px`),part(2097152,'{clk} ','{clk} ',`${state.countdown} s`),part(4194304,'像素码率 ','Pixels ',t?.pixelBitrate===undefined?'—':`${(t.pixelBitrate/1000000).toFixed(1)} Mb/s`),part(16777216,'绘制帧 ','Drawn ',t?.drawnFrames??'—'),part(33554432,'延迟帧 ','Late ',t?.lateFrames??'—')].filter(v=>v!==null);
}
// Load only glyphs that can actually be drawn in the current screen bars.
// Numbers are covered separately, so live telemetry never invalidates a font
// window merely because a frame counter or clock advances.
function screenBarFontText(state){
  const settings=deviceSettings||{},mask=settings.barFields??screenBarDefaults.barFields;
  const visible=playbackBars(state,settings),t=screenTelemetry,en=uiLanguage==='en';
  const text=['0123456789 .:/%—…-'];
  if(visible.header){
    if(mask&(1|2))text.push(t?.dateTime||'',t?.clockDate||'',t?.clockTime||'');
    if(mask&131072)text.push(t?.deviceName||settings.activeApName||settings.apName||(en?'Teleprompter-P4':'提词器P4'),en?'Teleprompter-P4':'提词器P4');
    if(mask&4)text.push(...['准备中','播放中','已暂停','已停止'].map(tr));
  if(state.loop){if(mask&2048)text.push(en?'Loop count:':'循环次数：');}
    else{
      const docs=typeof orderedLibraryItems==='function'?orderedLibraryItems():libraryState?.items||[],index=docs.findIndex(d=>d.title===libraryState?.active);
      if(mask&8388608)text.push(en?'Previous:':'上一篇:',t?.previousTitle??docs[index-1]?.title??'');
      if(state.autoNext&&(mask&8))text.push(en?'Next:':'下一篇:',t?.nextTitle??docs[index+1]?.title??'');
    }
  }
  if(visible.footer)text.push(...screenBarFields(state,mask).map(screenBarVisible));
  return text.join('');
}
function renderBarCapacity(){
  const note=document.getElementById('barCapacity');if(!note||!lastState)return;
  const settings=deviceSettings||{},width=lastState.rotation%180?1280:800;
  const font=Math.max(8,Math.min(settings.footerFontSize??screenBarDefaults.footerFontSize,Math.floor(((settings.footerHeight??screenBarDefaults.footerHeight)-8)/1.448)));
  previewContext.save();previewContext.font=`${font}px DeviceReader`;
  const controls=[...document.querySelectorAll('[data-bar-group="footer"] [data-bar-bit], [data-demo-bar]')].filter(input=>!headerBits.has(Number(input.dataset.barBit||input.dataset.demoBar)));
  const mask=controls.length?controls.filter(input=>input.checked).reduce((sum,input)=>sum|Number(input.dataset.barBit||input.dataset.demoBar),0):settings.barFields??screenBarDefaults.barFields;
  const fit=screenBarFit(screenBarFields(lastState,mask),width-32,previewContext);
  for(const input of controls){
    const bit=Number(input.dataset.barBit||input.dataset.demoBar),candidate=screenBarFields(lastState,mask|bit),result=screenBarFit(candidate,width-32,previewContext);
    const blocked=!input.checked&&(candidate.length>6||result.shown<candidate.length);
    if(input.hasAttribute('data-demo-bar'))input.disabled=blocked;
    else if(online&&!firmwareLocked()&&deviceSettings?.barFields!==undefined&&!busy&&!playbackPending&&!libraryActionPending)input.disabled=blocked;
    input.closest('label').dataset.barLimited=String(blocked);
  }
  previewContext.restore();
  note.textContent=uiLanguage==='en'?`Select up to 6 fields. ${fit.total} selected; ${fit.shown} fit on one line at this size.${fit.shown<fit.total?' Reduce font size or deselect fields to show all.':''}`:`最多选择 6 项，当前已选 ${fit.total} 项，本方向与字号下可显示 ${fit.shown} 项。${fit.shown<fit.total?'请减小字号或取消部分参数以全部显示。':'空间不足的组合无法继续勾选。'}`;
  const help=document.getElementById('barCapacityInfo');
  if(help){help.dataset.tooltip=note.textContent;if(help.getAttribute('aria-expanded')==='true')document.getElementById('tooltip').textContent=note.textContent;}

}
function drawScreenBars(state,width,height){
  const settings=deviceSettings||{},mask=settings.barFields??screenBarDefaults.barFields,header=settings.headerHeight??screenBarDefaults.headerHeight,footer=settings.footerHeight??screenBarDefaults.footerHeight;
  const ctx=previewContext,fontSize=(value,bar)=>Math.max(8,Math.min(value,Math.floor((bar-8)/1.448)));
  const headerFont=fontSize(settings.headerFontSize??screenBarDefaults.headerFontSize,header),footerFont=fontSize(settings.footerFontSize??screenBarDefaults.footerFontSize,footer);
  const draw=(raw,x,y,max,bar,font,align='left')=>{
    ctx.save();ctx.font=`${font}px DeviceReader`;ctx.textBaseline='alphabetic';ctx.fillStyle='#8aa4b8';
    let text=raw;while(text&&ctx.measureText(screenBarVisible(text)).width>max)text=Array.from(text.replace(/…$/,'')).slice(0,-1).join('')+'…';
    const visible=screenBarVisible(text),metrics=ctx.measureText(visible||'Ag');
    const inkHeight=metrics.actualBoundingBoxAscent+metrics.actualBoundingBoxDescent;
    const baseline=y+(bar-inkHeight)/2+metrics.actualBoundingBoxAscent,center=y+bar/2;
    const left=x-(align==='right'?metrics.width:align==='center'?metrics.width/2:0);
    ctx.beginPath();ctx.rect(x-(align==='right'?max:align==='center'?max/2:0),y+4,max,bar-8);ctx.clip();
    ctx.fillText(visible,left,baseline);
    for(let i=0;i<text.length;++i)if(text[i]==='|'){
      const sx=left+ctx.measureText(visible.slice(0,i)).width+ctx.measureText(' ').width/2,h=Math.max(1,Math.floor(inkHeight/2));
      ctx.fillStyle='#536572';ctx.fillRect(Math.round(sx),Math.round(center-h/2),1,h);ctx.fillStyle='#8aa4b8';
    }
    const tokens=/\{(sun|clk)\}/g;let match;
    while((match=tokens.exec(text))){
      const size=Math.min(inkHeight,ctx.measureText('     ').width-2),ix=left+ctx.measureText(visible.slice(0,match.index)).width+1,iy=center-size/2;
      ctx.strokeStyle='#8aa4b8';ctx.lineWidth=size>=18?2:1;ctx.lineCap='round';ctx.beginPath();
      ctx.arc(ix+size/2,iy+size/2,match[1]==='sun'?size/6:size/2-1,0,2*Math.PI);ctx.stroke();ctx.beginPath();
      for(const [a,b,c,d] of screenBarPaths[match[1]==='sun'?0:1]){ctx.moveTo(ix+a*size/20,iy+b*size/20);ctx.lineTo(ix+c*size/20,iy+d*size/20);}ctx.stroke();
    }
    ctx.restore();return {text:visible,center,inkHeight};
  };
  const telemetry=screenTelemetry,english=uiLanguage==='en';
  const stopped=typeof playbackStopped==='function'?playbackStopped(state):(state.stopped??!!state.playbackFinished);
  const docs=typeof orderedLibraryItems==='function'?orderedLibraryItems():libraryState?.items||[],index=docs.findIndex(d=>d.title===libraryState?.active);
  const next=telemetry?.nextTitle??docs[index+1]?.title,previous=telemetry?.previousTitle??docs[index-1]?.title;
  const visibility=playbackBars(state,settings);
  if(visibility.header){
    const radioName=telemetry?.deviceName||settings.activeApName||settings.apName||'';
    const titleName=settings.apNameDefault!==false&&/^(提词器(?:P4)?|Teleprompter-P4)(-[0-9a-f]{4})?$/i.test(radioName)?(english?'Teleprompter-P4':'提词器P4'):radioName||(english?'Teleprompter-P4':'提词器P4');
    if(mask&131072)draw(titleName,width/2,0,width/4,header,headerFont,'center');
    const dateTime=(telemetry?.dateTime||'').split('\n');
    const clock=[mask&2?(telemetry?.clockDate??dateTime[0]):'',mask&1?(telemetry?.clockTime??dateTime[1]??'').replace(/^(\d{2}:\d{2}):\d{2}$/, '$1'):''].filter(Boolean).join(' ');
    draw(clock,16,0,width*3/8-16,header,headerFont);
    const heading=[];
    if(mask&4)heading.push(tr(state.countdownRemainingMs?'准备中':state.playing?'播放中':stopped?'已停止':'已暂停'));
    if(state.loop){if(mask&2048)heading.push((english?'Loop count: ':'循环次数：')+(state.loopCount||0));}
    else{
      if((mask&8388608)&&previous)heading.push((english?'Previous: ':'上一篇: ')+previous);
      if(state.autoNext&&(mask&8)&&next)heading.push((english?'Next: ':'下一篇: ')+next);
    }
    const icon=headerFont,group=width*3/8-16;
    const elapsed=Math.max(0,(state.playbackElapsedMs||0)+(state.playing&&!state.countdownRemainingMs?Math.max(0,performance.now()-(typeof previewReceivedAt==='number'?previewReceivedAt:performance.now())):0));
    const seconds=Math.floor(elapsed/1000),timecode=(mask&67108864)&&state.length!==0?[Math.floor(seconds/3600),Math.floor(seconds/60)%60,seconds%60].map(n=>String(n).padStart(2,'0')).join(':'):'';
    ctx.font=`${headerFont}px DeviceReader`;
    const timecodeWidth=timecode?ctx.measureText(timecode).width+12:0;
    const row=draw(heading.join(' | '),width-16,0,group-icon-8-timecodeWidth,header,headerFont,'right');
    if(timecode){ctx.font=`${headerFont}px DeviceReader`;const right=width-16-ctx.measureText(row.text).width-((mask&4)&&!state.countdownRemainingMs?icon+8:0)-12;draw(timecode,right,0,timecodeWidth,header,headerFont,'right');}
    if((mask&4)&&!state.countdownRemainingMs){
      ctx.save();ctx.strokeStyle=ctx.fillStyle='#8aa4b8';ctx.lineWidth=2;
      ctx.font=`${headerFont}px DeviceReader`;const x=width-16-ctx.measureText(row.text).width-icon-8,top=row.center-icon/2;
      if(state.playing&&!state.countdownRemainingMs){
        // Match the device's small, opaque red indicator. Keep the text steady.
        const reduced=typeof matchMedia==='function'&&matchMedia('(prefers-reduced-motion: reduce)').matches;
        const bright=reduced||performance.now()%1200<600;
        ctx.fillStyle=bright?'#ef4444':'#782222';ctx.beginPath();ctx.arc(x+icon/2,row.center,Math.max(8,Math.floor(icon*3/5))/2,0,Math.PI*2);ctx.fill();
      }
      else if(stopped){ctx.fillRect(x+2,top+2,icon-4,icon-4);}
      else{ctx.fillRect(x+2,top+1,3,icon-2);ctx.fillRect(x+icon-4,top+1,3,icon-2);}ctx.restore();
    }
  }
  if(visibility.footer){
    ctx.save();ctx.font=`${footerFont}px DeviceReader`;ctx.textBaseline='alphabetic';
    const fields=screenBarFields(state,mask),fit=screenBarFit(fields,width-32,ctx),center=height-footer/2;
    let x=width-16-fit.width;
    for(let i=0;i<fit.shown;i++){
      if(i){const h=Math.max(6,Math.floor((settings.footerFontSize??screenBarDefaults.footerFontSize)/2));ctx.fillStyle='#536572';ctx.fillRect(Math.round(x+12),Math.round(center-h/2),1,h);x+=24;}
      const item=screenBarFieldMetrics(fields[i],ctx);
      if(item.type){
        const size=item.iconSize,ix=x,iy=center-size/2;
        ctx.strokeStyle='#8aa4b8';ctx.lineWidth=size>=16?2:1;ctx.lineCap='round';ctx.beginPath();
        const inset=item.type==='sun'?Math.floor(size/3):1,r=(size-2*inset-ctx.lineWidth)/2;
        ctx.arc(ix+size/2,iy+size/2,Math.max(1,r),0,2*Math.PI);ctx.stroke();ctx.beginPath();
        for(const [a,b,c,d] of screenBarPaths[item.type==='sun'?0:1]){ctx.moveTo(ix+Math.floor(a*size/20),iy+Math.floor(b*size/20));ctx.lineTo(ix+Math.floor(c*size/20),iy+Math.floor(d*size/20));}ctx.stroke();
        x+=size+6;
      }
      const metrics=ctx.measureText(item.text);
      ctx.fillStyle='#8aa4b8';ctx.fillText(item.text,x,center+(metrics.actualBoundingBoxAscent-metrics.actualBoundingBoxDescent)/2);
      x+=ctx.measureText(item.text).width;
    }
    ctx.restore();
  }
}
