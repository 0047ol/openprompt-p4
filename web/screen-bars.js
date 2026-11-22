// Screen chrome preferences are device settings shared by both transports.
const screenBarDefaults=/* SCREEN_BAR_DEFAULTS */;
const barOptions=[['时间',1],['日期',2],['设备名称',131072],['播放状态',4],['上一篇稿件',8388608],['下一篇稿件',8],['循环次数',2048],['播放倍率',16],['帧率',32],['缓冲',128],['帧间抖动',256],['绘制耗时',512],['播放进度',1024],['内存',4096],['字号',16384],['屏幕亮度',32768],['字间距',262144],['行间距',524288],['左右边距',1048576],['倒计时',2097152],['像素码率',4194304],['绘制帧数',16777216],['延迟帧数',33554432]];
barOptions.splice(3,0,['播放时间码',67108864]);
barOptions.splice(3,0,['显示版本',134217728]);
const headerBits=new Set([1,2,4,8,2048,131072,8388608,67108864,134217728]);
const exclusiveBarGroups=[[8,8388608]];
function selectExclusiveBar(input,selector='[data-bar-bit]'){
  if(!input.checked)return;
  const bit=Number(input.dataset.barBit||input.dataset.demoBar),group=exclusiveBarGroups.find(group=>group.includes(bit));
  for(const other of document.querySelectorAll(selector)){
    const otherBit=Number(other.dataset.barBit||other.dataset.demoBar);
    if(other!==input&&((group&&group.includes(otherBit))||(bit===67108864&&(otherBit===1||otherBit===2))||((bit===1||bit===2)&&otherBit===67108864)))other.checked=false;
  }
}
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
barsSection.querySelector('h3').textContent='播放控制';
barsSection.querySelector('.wifi-body').insertAdjacentHTML('afterbegin','<label class="check switch multi-control-row"><input id="multiControl" type="checkbox" role="switch" data-device disabled>开启多人控制</label>');
barsSection.querySelector('#multiControl').onchange=()=>saveImmediateSetting({multiControl:$('multiControl').checked});
const headerExplanation=barsSection.querySelector('.wifi-body').lastElementChild;
headerExplanation.textContent='标题居中；日期与时间可同时显示，与播放时间码互斥，显示在左上角。上一篇与下一篇二选一。循环时右上角显示已循环次数；自动播放下一篇时显示下一篇。';
const headerHelp=document.createElement('button');headerHelp.type='button';headerHelp.className='info-button';headerHelp.setAttribute('aria-label',tr('说明'));headerHelp.dataset.tooltip=headerExplanation.textContent;
barsSection.querySelector('[data-bar-group=header] legend').append(headerHelp);headerExplanation.remove();
const capacityNote=barsSection.querySelector('#barCapacity');capacityNote.hidden=true;
const capacityHelp=document.createElement('button');capacityHelp.id='barCapacityInfo';capacityHelp.type='button';capacityHelp.className='info-button';capacityHelp.setAttribute('aria-label',tr('说明'));capacityHelp.dataset.tooltip='最多选择 6 项。';
barsSection.querySelector('[data-bar-group=footer] legend').append(capacityHelp);
for(const [id,,label,initial,min,max,step] of sharedBarControls){
  barsSection.querySelector('.bar-heights').insertAdjacentHTML('beforeend',`<div class="bar-height-control"><label for="${id}"><span class="bar-height-name">${label}</span><span class="bar-height-value"><output id="${id}Value">${initial}</output> px</span></label><div class="numeric-control"><input id="${id}" type="range" min="${min}" max="${max}" step="${step}" value="${initial}" data-device disabled><input id="${id}Number" type="number" aria-label="${label}" min="${min}" max="${max}" step="${step}" value="${initial}" data-device disabled></div></div>`);
}
$('systemSettings').after(barsSection);
// Keep field selection inside the shared section so language changes and
// device support checks use the same controls in the compact dialogs.
const barFieldActions=document.createElement('div');barFieldActions.className='bar-field-actions';
barsSection.querySelector('.bar-heights').after(barFieldActions);
for(const [group,label] of [['header','标题栏内容'],['footer','状态栏内容']]){
  const fields=barsSection.querySelector(`[data-bar-group=${group}]`),dialog=document.createElement('dialog');
  dialog.id=group+'FieldsDialog';dialog.className='content-dialog bar-fields-dialog';
  dialog.setAttribute('aria-labelledby',group+'FieldsTitle');
  dialog.innerHTML=`<h3 id="${group}FieldsTitle">${label}</h3><div class="actions dialog-footer"><button type="button" class="ghost" data-bar-reset="${group}" data-device disabled>恢复默认</button><button type="button" data-bar-done>完成</button></div>`;
  dialog.querySelector('.actions').before(fields);barsSection.append(dialog);
  const button=document.createElement('button');button.type='button';button.textContent='设置'+label;button.dataset.barDialog=dialog.id;button.disabled=true;
  button.onclick=()=>{if(button.disabled||!online||firmwareLocked()||devicePowerBlocked())return;dialog.showModal();motionEnter(dialog,'dialog');};barFieldActions.append(button);
  dialog.querySelector('[data-bar-done]').onclick=()=>dialog.close();
  dialog.querySelector('[data-bar-reset]').onclick=()=>{
    if(!online||firmwareLocked()||devicePowerBlocked())return;
    const groupMask=barOptions.filter(([,bit])=>headerBits.has(bit)===(group==='header')).reduce((mask,[,bit])=>mask|bit,0);
    const mask=((deviceSettings.barFields??screenBarDefaults.barFields)&~groupMask)|(screenBarDefaults.barFields&groupMask);
    void saveImmediateSetting({barFields:mask});
  };
}
for(const [label,bit] of barOptions){const el=document.createElement('label');el.className='check';el.innerHTML=`<input type="checkbox" data-bar-bit="${bit}" data-device disabled><span>${label}</span>`;barsSection.querySelector(`[data-bar-group="${headerBits.has(bit)?'header':'footer'}"]`).append(el);el.querySelector('input').onchange=event=>{selectExclusiveBar(event.target);const mask=[...barsSection.querySelectorAll('[data-bar-bit]:checked')].reduce((sum,input)=>sum+Number(input.dataset.barBit),0);renderBarCapacity();void saveImmediateSetting({barFields:mask});};}
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
    const viewOnly=deviceSettings?.multiControl!==undefined&&playbackAccess.epoch===epoch&&!playbackAccess.allowed;
    setControlDisabled(input,viewOnly||!online||firmwareLocked()||devicePowerBlocked()||deviceSettings?.barFields===undefined||unsupported||newField||(key==='multiControl'&&deviceSettings?.multiControl===undefined),busy||playbackPending||libraryActionPending);
  }
  for(const button of section.querySelectorAll('[data-bar-reset],[data-bar-dialog]'))setControlDisabled(button,!online||firmwareLocked()||devicePowerBlocked()||deviceSettings?.barFields===undefined,busy||libraryActionPending);
  renderBarCapacity();
}
renderDeviceSettings=function(settings){
  renderSettingsWithBars(settings);
  $('multiControl').checked=!!settings.multiControl;
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
function screenBarVisible(text){return String(text).replaceAll('|',' ').replace(/\{(?:sun|clk|prv|nxt|fwd|rev|txt)\}/g,'     ');}
const screenBarIconTypes={sun:0,clk:1,prv:2,nxt:3,fwd:4,rev:5,txt:6};
const screenBarPaths=[
  [[10,1,10,3],[10,17,10,19],[1,10,3,10],[17,10,19,10],[4,4,5,5],[15,15,16,16],[4,16,5,15],[15,5,16,4]],
  [[10,5,10,10],[10,10,14,12]],
  [[2,1,2,19],[18,1,4,10],[4,10,18,19],[18,19,18,1]],
  [[18,1,18,19],[2,1,16,10],[16,10,2,19],[2,19,2,1]],
  [[1,1,9,10],[9,10,1,19],[1,19,1,1],[11,1,19,10],[19,10,11,19],[11,19,11,1]],
  [[19,1,11,10],[11,10,19,19],[19,19,19,1],[9,1,1,10],[1,10,9,19],[9,19,9,1]],
  [[2,2,18,2],[10,2,10,18]]
];
function screenBarDrawIcon(ctx,type,x,center,size){
  const index=screenBarIconTypes[type],y=center-size/2;
  ctx.strokeStyle='#8aa4b8';ctx.lineWidth=size>=16?2:1;ctx.lineCap='round';
  if(index<2){
    const inset=index===0?Math.floor(size/3):1,r=(size-2*inset-ctx.lineWidth)/2;
    ctx.beginPath();ctx.arc(x+size/2,center,Math.max(1,r),0,2*Math.PI);ctx.stroke();
  }
  ctx.beginPath();
  for(const [a,b,c,d] of screenBarPaths[index]){ctx.moveTo(x+Math.floor(a*size/20),y+Math.floor(b*size/20));ctx.lineTo(x+Math.floor(c*size/20),y+Math.floor(d*size/20));}ctx.stroke();
}
function screenBarFieldMetrics(field,ctx){
  const match=/^\{(sun|clk|fwd|rev|txt)\} /.exec(field),font=parseFloat(ctx.font)||22;
  const iconSize=Math.max(8,Math.floor(font)),text=match?field.slice(6):field;
  return {text,type:match?match[1]:null,iconSize,width:ctx.measureText(text).width+(match?iconSize+6:0)};
}
function screenBarGap(ctx){return Math.max(8,Math.min(24,(Math.floor((parseFloat(ctx.font)||22)*3/4)&~1)));}
function screenBarFit(fields,width,ctx){
  for(let count=Math.min(fields.length,6);count>=0;--count){
    const text=fields.slice(0,count).join(' | ');
    const measured=fields.slice(0,count).reduce((sum,field,i)=>sum+screenBarFieldMetrics(field,ctx).width+(i?screenBarGap(ctx):0),0);
    if(measured<=width||!count)return {text,shown:count,total:fields.length,width:measured};
  }
}
function screenBarFitAll(fields,width,ctx,font,key){
  const cache=screenBarFitAll.cache||(screenBarFitAll.cache=new Map());
  let size=Math.min(font,cache.get(key)??font),fit;
  for(;;){ctx.font=`${size}px DeviceReader`;fit=screenBarFit(fields,width,ctx);if(fit.shown===fields.length||size<=8)break;--size;}
  // Keep the selected values even at the minimum size. Capacity diagnostics
  // expose an exceptional overflow instead of silently removing a field.
  const measured=fields.reduce((sum,field,i)=>sum+screenBarFieldMetrics(field,ctx).width+(i?screenBarGap(ctx):0),0);
  fit={text:fields.join(' | '),shown:fields.length,total:fields.length,width:measured,overflow:measured>width};
  cache.set(key,size);if(cache.size>16)cache.delete(cache.keys().next().value);
  return {...fit,fontSize:size};
}
function screenBarFields(state,mask){
  const t=screenTelemetry,en=uiLanguage==='en',part=(bit,zh,eng,value)=>mask&bit?(en?eng:zh)+(value??''):null;
  const memory=t?.totalMemory?`${(t.freeMemory/1048576).toFixed(1)}/${(t.totalMemory/1048576).toFixed(1)} MB`:'—';
  return [state.pageMode===1?null:part(16,state.shuttle<0?'{rev} ':'{fwd} ',state.shuttle<0?'{rev} ':'{fwd} ',`x${Math.abs(state.shuttle||1)}`),part(32,'','',`${t?.fps?.toFixed(1)??'—'} fps`),part(128,'缓冲 ','Buffer ',t?`${t.bufferReady}/${t.bufferTotal}`:'—'),part(256,'抖动 ','Jitter ',`${t?.jitterMs?.toFixed(1)??'—'} ms`),part(512,'绘制 ','Render ',`${t?.renderMs?.toFixed(1)??'—'} ms`),part(1024,'','',state.pageMode===1?pageNumberText(state):`${Math.min(100,Math.round((state.scroll||0)/Math.max(1,previewLayout?.endScroll||1)*100))}%`),part(4096,'RAM ','RAM ',memory),part(16384,'{txt} ','{txt} ',`${state.font} px`),part(32768,'{sun} ','{sun} ',`${state.brightness}%`),part(262144,'字距 ','Letter ',`${state.letterSpacing} px`),part(524288,'行距 ','Line ',`${state.lineSpacing} px`),part(1048576,'边距 ','Margins ',`${state.marginLeft}/${state.marginRight} px`),state.countdown>0?part(2097152,'{clk} ','{clk} ',`${state.countdown} s`):null,part(4194304,'像素码率 ','Pixels ',t?.pixelBitrate===undefined?'—':`${(t.pixelBitrate/1000000).toFixed(1)} Mb/s`),part(16777216,'绘制帧 ','Drawn ',t?.drawnFrames??'—'),part(33554432,'延迟帧 ','Late ',t?.lateFrames??'—')].filter(v=>v!==null);
}
// Load only glyphs that can actually be drawn in the current screen bars.
// Numbers are covered separately, so live telemetry never invalidates a font
// window merely because a frame counter or clock advances.
function screenBarFontText(state){
  const settings=deviceSettings||{},mask=settings.barFields??screenBarDefaults.barFields;
  const visible=playbackBars(state,settings),t=screenTelemetry,en=uiLanguage==='en';
  const text=['0123456789 .:/%—…-()'];
  if(visible.header){
    if(mask&(1|2))text.push(t?.dateTime||'',t?.clockDate||'',t?.clockTime||'');
    if(mask&131072)text.push(t?.deviceName||settings.activeApName||settings.apName||(en?'Teleprompter-P4':'提词器P4'),en?'Teleprompter-P4':'提词器P4');
    if(mask&4)text.push(...['准备中','播放中','已暂停','已停止'].map(tr));
    if(state.pageMode===1)text.push(pageProgressText(state));
  if(state.loop){if(mask&2048)text.push(en?'Loops':'已循环次');}
    else{
      const docs=typeof orderedLibraryItems==='function'?orderedLibraryItems():libraryState?.items||[],index=docs.findIndex(d=>d.title===libraryState?.active);
      if(mask&8388608)text.push(t?.previousTitle??docs[index-1]?.title??'');
      if(state.autoNext&&(mask&8))text.push(t?.nextTitle??docs[index+1]?.title??'');
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
  const fit=screenBarFitAll(screenBarFields(lastState,mask),width-32,previewContext,font,`capacity:${width}:${font}:${mask}:${uiLanguage}`);
  for(const input of controls){
    const bit=Number(input.dataset.barBit||input.dataset.demoBar),candidate=screenBarFields(lastState,mask|bit);
    const selected=controls.filter(control=>control.checked).length;
    const blocked=!input.checked&&selected>=6;
    if(input.hasAttribute('data-demo-bar'))input.disabled=blocked;
    else if(online&&!firmwareLocked()&&deviceSettings?.barFields!==undefined&&!busy&&!playbackPending&&!libraryActionPending)input.disabled=blocked;
    input.closest('label').dataset.barLimited=String(blocked);
  }
  previewContext.restore();
  const selected=controls.length?controls.filter(input=>input.checked).length:fit.total;
  note.textContent=uiLanguage==='en'?`Select up to 6 fields. All selected fields stay on one line; the font is reduced when needed (minimum 8 px). Countdown is hidden at 0. ${selected} selected; current size ${fit.fontSize} px.`:`最多选择 6 项，全部在一行完整显示；空间不足时自动缩小字号，最小 8 px。倒计时为 0 时不显示。当前已选 ${selected} 项，显示字号 ${fit.fontSize} px。`;
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
    const tokens=/\{(sun|clk|prv|nxt|fwd|rev|txt)\}/g;let match;
    while((match=tokens.exec(text))){
      const reserved=ctx.measureText('     ').width,size=Math.max(8,Math.floor(font));
      const ix=left+ctx.measureText(visible.slice(0,match.index)).width+(reserved-size)/2;
      screenBarDrawIcon(ctx,match[1],ix,center,size);
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
    let titleName=settings.apNameDefault!==false&&/^(提词器(?:P4)?|Teleprompter-P4)(-[0-9a-f]{4})?$/i.test(radioName)?(english?'Teleprompter-P4':'提词器P4'):radioName||(english?'Teleprompter-P4':'提词器P4');
    const version=telemetry?.version||(typeof firmwareState!=='undefined'?firmwareState?.version:null)||(typeof webProductVersion==='string'?webProductVersion:'');
    if((mask&134217728)&&version)titleName+=' ('+version+')';
    if(mask&131072)draw(titleName,width/2,0,width/4,header,headerFont,'center');
    const dateTime=(telemetry?.dateTime||'').split('\n');
    const elapsed=Math.max(0,(state.playbackElapsedMs||0)+(state.playing&&!state.countdownRemainingMs?Math.max(0,performance.now()-(typeof previewReceivedAt==='number'?previewReceivedAt:performance.now())):0));
    const seconds=Math.floor(elapsed/1000);
    const timecode=(mask&67108864)&&state.length!==0?[Math.floor(seconds/3600),Math.floor(seconds/60)%60,seconds%60].map(n=>String(n).padStart(2,'0')).join(':'):'';
    const clock=timecode||[mask&2?(telemetry?.clockDate??dateTime[0]??''):'',mask&1?(telemetry?.clockTime??dateTime[1]??'').replace(/^(\d{2}:\d{2}):\d{2}$/, '$1'):''].filter(Boolean).join(' ');
    draw(clock,16,0,width*3/8-16,header,headerFont);
    const heading=[];
    if(mask&4)heading.push(tr(state.countdownRemainingMs?'准备中':state.playing?'播放中':stopped?'已停止':'已暂停'));
    if(state.pageMode===1&&!state.playbackFinished&&state.pageCount)heading.push(pageProgressText(state));
    else if(!state.pageMode&&state.loop){if(mask&2048)heading.push((english?'Loops ':'已循环')+(state.loopCount||0)+(english?'':'次'));}
    else{
      if((mask&8388608)&&previous)heading.push('{prv} '+previous);
      else if(!state.pageMode&&state.autoNext&&(mask&8)&&next)heading.push('{nxt} '+next);
    }
    const icon=headerFont,group=width*3/8-16;
    ctx.font=`${headerFont}px DeviceReader`;
    // Drop optional header fields whole, matching the device, instead of
    // truncating a loop label before its number or half an adjacent title.
    while(heading.length&&ctx.measureText(screenBarVisible(heading.join(' | '))).width>group-icon-8)heading.pop();
    const row=draw(heading.join(' | '),width-16,0,group-icon-8,header,headerFont,'right');
    if((mask&4)&&!state.countdownRemainingMs&&state.playing){
      ctx.save();ctx.strokeStyle=ctx.fillStyle='#8aa4b8';ctx.lineWidth=2;
      ctx.font=`${headerFont}px DeviceReader`;const x=width-16-ctx.measureText(row.text).width-icon-8,top=row.center-icon/2;
      if(state.playing&&!state.countdownRemainingMs){
        // Match the device's small, opaque red indicator. Keep the text steady.
        const reduced=typeof matchMedia==='function'&&matchMedia('(prefers-reduced-motion: reduce)').matches;
        const bright=reduced||performance.now()%1200<600;
        ctx.fillStyle=bright?'#ef4444':'#782222';ctx.beginPath();ctx.arc(x+icon/2,row.center,Math.max(8,Math.floor(icon*3/5))/2,0,Math.PI*2);ctx.fill();
      }
      ctx.restore();
    }
  }
  if(visibility.footer){
    ctx.save();ctx.font=`${footerFont}px DeviceReader`;ctx.textBaseline='alphabetic';
    const fields=screenBarFields(state,mask),fit=screenBarFitAll(fields,width-32,ctx,footerFont,`draw:${width}:${footerFont}:${mask}:${uiLanguage}`),center=height-footer/2;
    let x=width-16-fit.width;
    for(let i=0;i<fit.shown;i++){
      if(i){const h=Math.max(6,Math.floor(fit.fontSize/2)),gap=screenBarGap(ctx);ctx.fillStyle='#536572';ctx.fillRect(Math.round(x+gap/2),Math.round(center-h/2),1,h);x+=gap;}
      const item=screenBarFieldMetrics(fields[i],ctx);
      if(item.type){
        screenBarDrawIcon(ctx,item.type,x,center,item.iconSize);
        x+=item.iconSize+6;
      }
      const metrics=ctx.measureText(item.text);
      ctx.fillStyle='#8aa4b8';ctx.fillText(item.text,x,center+(metrics.actualBoundingBoxAscent-metrics.actualBoundingBoxDescent)/2);
      x+=ctx.measureText(item.text).width;
    }
    ctx.restore();
  }
}
