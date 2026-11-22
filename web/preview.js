/* The device owns line breaks, glyph advances and scroll position. The page
 * draws that layout with the same version of the device font, in device pixels. */
function createPreviewMotion(){
  let previous=null,anchor=0,at=0,velocity=0,correction=0,duration=300,clockOffset=Infinity,pageSlideStarted=-Infinity;
  const position=now=>{
    if(previous?.pageMode===1)return anchor-(previous.pageTransition?32*Math.max(0,1-(now-pageSlideStarted)/180):0);
    const elapsed=Math.max(0,Math.min(2000,now-at));
    return Math.max(0,anchor+velocity*elapsed/1000+correction*Math.min(1,elapsed/duration));
  };
  return {position,update(state,now){
    // Status can arrive before its matching layout. Keep the last confirmed
    // loop length for this manuscript until its replacement arrives, including
    // a seam during that gap. Losing it subtracts every completed loop.
    if(state.loop&&!(state.loopPeriod>0)&&previous?.loop&&
      state.boot===previous.boot&&state.scriptRevision===previous.scriptRevision)
      state={...state,loopPeriod:previous.loopPeriod};
    // A layout publication can follow a control acknowledgement or change only
    // the bars. Modern devices explicitly report coordinate resets separately;
    // resetting again on a delayed layout publication makes forward text jump.
    const legacyLayoutReset=previous&&state.previewRevision!==previous.previewRevision&&
      (state.rewindRevision===undefined||previous.rewindRevision===undefined);
    const restart=!previous||state.boot!==previous.boot||legacyLayoutReset||
      state.scriptRevision!==previous.scriptRevision||state.loop!==previous.loop||
      (state.loop&&state.loopPeriod!==previous.loopPeriod)||state.rewindRevision!==previous.rewindRevision;
    if(!previous||state.boot!==previous.boot)clockOffset=Infinity;
    let lag=0;
    if(Number.isFinite(state.scrollSampleMs)){
      // Minimum observed clock offset excludes variable network queuing.
      // A 32-bit device clock wrapping is a new timing baseline.
      if(previous&&state.scrollSampleMs<previous.scrollSampleMs&&previous.scrollSampleMs-state.scrollSampleMs>0x80000000)clockOffset=Infinity;
      clockOffset=Math.min(clockOffset,now-state.scrollSampleMs);
      lag=Math.max(0,Math.min(2000,now-state.scrollSampleMs-clockOffset));
    }
    if(state.pageMode===1){
      const stride=state.pageStride??(previous?.scriptRevision===state.scriptRevision?previous.pageStride:undefined);
      anchor=Number.isFinite(stride)?(state.pageIndex||0)*stride:(state.scroll||0);velocity=0;correction=0;at=now;
      if(state.pageChangedMs&&(!previous||previous.pageChangedMs!==state.pageChangedMs||previous.boot!==state.boot)){
        const age=Number.isFinite(state.scrollSampleMs)?Math.max(0,((state.scrollSampleMs-state.pageChangedMs)|0)+lag):0;
        pageSlideStarted=now-age;
      }else if(!state.pageChangedMs)pageSlideStarted=-Infinity;
      previous={...state,pageStride:stride};return;
    }
    const nextVelocity=state.playing&&!state.countdownRemainingMs?state.speed*(state.shuttle||1):0;
    let target=Math.max(0,(state.scroll||0)+nextVelocity*lag/1000);
    if(state.loop&&state.loopPeriod)target+=(state.loopCount||0)*state.loopPeriod;
    if(previous&&state.boot===previous.boot&&state.rewindRevision!==previous.rewindRevision&&state.rewindRevision!==undefined&&state.seekRevision!==state.rewindRevision)target=0;
    const current=position(now);
    if(restart||!nextVelocity){anchor=target;correction=0;}
    else{
      anchor=current;correction=target-current;
      // Gradually correct latency error without moving against playback.
      // Even a late sample may adjust speed by at most half the set speed.
      duration=Math.max(300,Math.abs(correction)*2000/Math.abs(nextVelocity));
    }
    velocity=nextVelocity;at=now;previous=state;
  }};
}
const previewMotion=createPreviewMotion();
// Keep one device-clock deadline for an entire countdown. Re-anchoring to each
// response's arrival time made late packets replay digits and grow the ring.
function createPreviewCountdown(){
  let previous=null,deadline=0,clockOffset=Infinity;
  return {
    remaining:now=>deadline?Math.max(0,deadline-now):0,
    update(state,now){
      const reset=!previous||state.boot!==previous.boot||state.scriptRevision!==previous.scriptRevision||
        state.rewindRevision!==previous.rewindRevision;
      if(!previous||state.boot!==previous.boot||
        (Number.isFinite(state.scrollSampleMs)&&Number.isFinite(previous.scrollSampleMs)&&
          previous.scrollSampleMs-state.scrollSampleMs>0x80000000))clockOffset=Infinity;
      let lag=0;
      if(Number.isFinite(state.scrollSampleMs)){
        clockOffset=Math.min(clockOffset,now-state.scrollSampleMs);
        lag=Math.max(0,Math.min(2000,now-state.scrollSampleMs-clockOffset));
      }
      if(state.countdownRemainingMs>0){
        const candidate=now+Math.max(0,state.countdownRemainingMs-lag);
        deadline=reset||!previous.countdownRemainingMs?candidate:Math.min(deadline,candidate);
      }else deadline=0;
      previous=state;
      return deadline;
    }
  };
}
const previewCountdown=createPreviewCountdown();
var previewLayout=null,previewReceivedAt=0,previewLayoutLoading=false;
let previewGeneration=0;
let previewSelectionRevision=null;
const previewCanvas=document.createElement('canvas');
previewCanvas.id='previewCanvas';previewCanvas.hidden=true;
$('preview').append(previewCanvas);
const previewContext=previewCanvas.getContext('2d');
function resetDevicePreview(selecting=false){
  ++previewGeneration;previewLayout=null;previewDrawSignature='';
  if(selecting)previewSelectionRevision=lastState?.previewRevision??null;
  else if(pageName!=='play')previewSelectionRevision=null;
  previewCanvas.hidden=true;previewContext.clearRect(0,0,previewCanvas.width,previewCanvas.height);
  previewFontReady=false;previewFontLoading=false;previewFontError='';previewFontRetryAt=0;
  previewFontText='';previewPreloadKey='';
  if($('previewStatus'))$('previewStatus').hidden=true;
  $('previewLoading').hidden=false;
}
function previewMatchesDocument(layout){
  return !!layout&&!!lastState&&layout.boot===lastState.boot&&
    layout.revision===lastState.previewRevision&&layout.revision!==previewSelectionRevision&&
    (layout.scriptRevision===undefined||lastState.scriptRevision===undefined||layout.scriptRevision===lastState.scriptRevision);
}
function fitDevicePreview(){
  if(pageName!=='play'||$('playWorkspace').hidden)return;
  const frame=$('preview'),card=frame.closest('.preview-card'),style=getComputedStyle(card);
  const portrait=previewCanvas.hidden?(lastState?.rotation??90)%180===0:previewCanvas.width<previewCanvas.height;
  syncPlaybackSheetDefault((lastState?.rotation??90)%180===0);
  if(document.body.dataset.playSettingsFullscreen==='true')return;
  const ratio=portrait?800/1280:1280/800;
  const availableWidth=card.clientWidth-parseFloat(style.paddingLeft)-parseFloat(style.paddingRight)-2;
  const viewportHeight=window.visualViewport?.height||innerHeight;
  const mobile=sheetViewport.matches;
  const heading=card.querySelector('.card-head');
  const availableHeight=mobile?Math.max(1,card.clientHeight-parseFloat(style.paddingTop)-parseFloat(style.paddingBottom)-(heading?.getBoundingClientRect().height||0)-10):Math.max(160,viewportHeight-frame.getBoundingClientRect().top-scrollY-parseFloat(style.paddingBottom)-22);
  const width=Math.max(1,Math.floor(Math.min(availableWidth,availableHeight*ratio)));
  const height=Math.floor(width/ratio);
  if(frame.style.width!==width+'px')frame.style.width=width+'px';
  if(frame.style.height!==height+'px')frame.style.height=height+'px';
  const workspace=$('playWorkspace');
  if(!mobile&&innerWidth>960){
    // Keep settings at the portrait reference height. Only the preview card
    // follows the actual screen direction; neither card stretches the other.
    const portraitWidth=Math.max(1,Math.floor(Math.min(availableWidth,availableHeight*800/1280)));
    const portraitHeight=Math.floor(portraitWidth*1280/800);
    const chrome=card.getBoundingClientRect().height-frame.getBoundingClientRect().height;
    // frame.style.height is already border-box; adding its border again made
    // the settings card two pixels taller than the portrait preview.
    const referenceHeight=Math.ceil(portraitHeight+chrome)+'px';
    if(workspace.style.getPropertyValue('--play-settings-reference-height')!==referenceHeight)workspace.style.setProperty('--play-settings-reference-height',referenceHeight);
  }else workspace.style.removeProperty('--play-settings-reference-height');
}
new ResizeObserver(fitDevicePreview).observe($('preview').closest('.preview-card'));
window.addEventListener('resize',fitDevicePreview);window.visualViewport?.addEventListener('resize',fitDevicePreview);
let previewFontReady=false,previewDrawSignature='',previewFontLoading=false,previewFontRetryAt=0;
let previewFontText='',previewFontError='',previewPreloadKey='';
let previewFontWindow='';
function ensurePreviewFont(){
  if(!previewLayout)return;
  const layout=previewLayout,state=lastState;
  if(!state)return;
  const vh=layout.height??((layout.rotation%180===0?1280:800)-200),step=layout.lineHeight+layout.lineSpacing;
  const scroll=state.scroll||0,top=layout.textTop??Math.floor(vh/4);
  const first=state.pageMode===1?(state.pageIndex||0)*(layout.pageRows||1):Math.max(0,Math.floor((scroll-top-layout.lineHeight)/step));
  const windowKey=[previewGeneration,layout.revision,first,state.loop,uiLanguage,screenTelemetry?.nextTitle,screenTelemetry?.previousTitle,screenTelemetry?.deviceName,screenTelemetry?.footerText].join(':');
  if(previewFontReady&&previewFontWindow===windowKey)return;
  previewFontWindow=windowKey;
  const visible=layout.rows.slice(first,first+Math.ceil(vh/step)+4).concat(state.loop?layout.rows.slice(0,Math.ceil(vh/step)+4):[]).map(row=>row.text).join('');
  // Compare glyph sets, not changing telemetry strings. The exact same font
  // faces remain ready while numbers, clocks and already-loaded words change.
  const text=[...new Set(visible+screenBarFontText(state))].sort().join('');
  if(previewFontText!==text){previewFontText=text;previewFontReady=false;}
  if(previewFontReady||previewFontLoading||performance.now()<previewFontRetryAt)return;
  previewFontLoading=true;
  const selection=previewGeneration;
  loadReaderTextFont(text).then(()=>{
      if(selection!==previewGeneration)return;
      previewFontReady=previewFontText===text;previewFontError='';
      const key=layout.revision+':'+first;
      if(previewPreloadKey!==key){previewPreloadKey=key;void loadReaderTextFont(layout.rows.slice(first,first+Math.ceil(vh/step)+24).map(row=>row.text).join('')).catch(()=>{});}
    })
    .catch(()=>{if(selection===previewGeneration){previewFontRetryAt=performance.now()+5000;previewFontError=tr('预览字体暂未加载完成，正在重试；真机显示不受影响。');}})
    .finally(()=>{if(selection===previewGeneration)previewFontLoading=false;});
}

async function refreshPreviewLayout(){
  if(pageName!=='play'||pageTransition?.name==='play'||playbackDocumentPending())return;
  if(previewLayoutLoading||!lastState?.previewRevision||previewMatchesDocument(previewLayout))return;
  previewLayoutLoading=true;
  const version=epoch,boot=lastState.boot,generation=writeGeneration,selection=previewGeneration,title=libraryState?.active;
  try{
    const current=()=>pageName==='play'&&version===epoch&&generation===writeGeneration&&selection===previewGeneration&&title===libraryState?.active&&!liveControls.size;
    const pending=queue.then(()=>current()?libraryRequest({op:'preview'},current):null);
    queue=pending.catch(()=>{});
    const layout=await pending;
    if(layout?.ok&&current()&&boot===lastState?.boot){
      layout.boot=boot;
      if(!previewMatchesDocument(layout))return;
      layout.offsets=new Map(layout.advances.map(([cp,width,offset])=>[cp,offset||0]));
      layout.advances=new Map(layout.advances.map(([cp,width])=>[cp,width]));
      const bytes=encoder.encode(layout.text);
      layout.rows=layout.lines.map(([start,length,width])=>({text:decoder.decode(bytes.slice(start,start+length)).replace(/[\r\n]+$/,''),width}));
      previewSelectionRevision=null;
      previewLayout=layout;previewMotion.update({...lastState,loopPeriod:layout.loopPeriod,pageStride:layout.pageRows*(layout.lineHeight+layout.lineSpacing)},performance.now());previewDrawSignature='';preview();
    }
  }catch{}finally{previewLayoutLoading=false;}
}

function readerText(text,x,y,size,color,spacing=0,center=false){
  const ctx=previewContext;ctx.font=`${size}px DeviceReader`;ctx.fontKerning='none';ctx.textBaseline='alphabetic';ctx.fillStyle=color;
  const chars=Array.from(text),advance=char=>Math.round(ctx.measureText(char).width);
  if(center)x-=Math.floor((chars.reduce((sum,char)=>sum+advance(char),0)+Math.max(0,chars.length-1)*spacing)/2);
  for(const char of chars){ctx.fillText(char,x,y);x+=advance(char)+spacing;}
}

// Rasterize a visible line once using the device's exact advances. Reusing
// these pixels avoids shaping and painting every glyph on every scroll frame.
const readerRowCache=new Map();
let readerRowLayout=null,readerRowStyle='',readerRowBytes=0;
function drawReaderRow(layout,state,index,top,viewWidth,color){
  const mirrored=!!(state.mirror&2),style=[color,state.bold,state.textAlign,viewWidth,mirrored].join(':');
  if(readerRowLayout!==layout||readerRowStyle!==style){
    readerRowCache.clear();readerRowBytes=0;readerRowLayout=layout;readerRowStyle=style;
  }
  const row=layout.rows[index];if(!row?.text)return;
  let cached=readerRowCache.get(index);
  if(!cached){
    const canvas=document.createElement('canvas');canvas.width=viewWidth;canvas.height=layout.lineHeight;
    const ctx=canvas.getContext('2d');ctx.font=`${layout.font}px DeviceReader`;ctx.fontKerning='none';ctx.textBaseline='alphabetic';ctx.fillStyle=color;
    if(mirrored){ctx.translate(0,canvas.height);ctx.scale(1,-1);}
    let x=state.textAlign===1?Math.floor((viewWidth-row.width)/2):state.textAlign===2?viewWidth-row.width:0;
    for(const char of row.text){
      const cp=char.codePointAt(0),advance=layout.advances.get(cp)||0;
      if(advance){const glyphX=x-(layout.offsets?.get(cp)||0);ctx.fillText(char,glyphX,layout.baseline);if(state.bold)ctx.fillText(char,glyphX+1,layout.baseline);x+=advance+layout.letterSpacing;}
    }
    const bytes=canvas.width*canvas.height*4;
    while(readerRowCache.size&&readerRowBytes+bytes>8*1024*1024){
      const oldest=readerRowCache.keys().next().value;readerRowBytes-=readerRowCache.get(oldest).bytes;readerRowCache.delete(oldest);
    }
    cached={canvas,bytes};readerRowCache.set(index,cached);readerRowBytes+=bytes;
  }else{readerRowCache.delete(index);readerRowCache.set(index,cached);}
  if(mirrored){previewContext.save();previewContext.translate(0,top+cached.canvas.height);previewContext.scale(1,-1);previewContext.drawImage(cached.canvas,0,0);previewContext.restore();}
  else previewContext.drawImage(cached.canvas,0,top);
}

function drawDevicePreview(now){
  requestAnimationFrame(drawDevicePreview);
  if(pageName!=='play'||(libraryState&&!libraryState.active))return;
  if(document.body.dataset.playSettingsFullscreen==='true')return;
  if(pageTransition?.name==='play')return;
  ensurePreviewFont();
  const status=previewLifecycle();
  let statusPanel=$('previewStatus');
  if(!statusPanel){statusPanel=document.createElement('div');statusPanel.id='previewStatus';statusPanel.className='preview-status';statusPanel.setAttribute('role','status');$('preview').append(statusPanel);}
  statusPanel.hidden=!status;
  if(status){
    statusPanel.textContent=tr(status);$('previewLoading').hidden=true;
    return;
  }
  if(!lastState||!previewLayout||!previewFontReady){
    if(previewFontError){statusPanel.hidden=false;statusPanel.textContent=previewFontError;$('previewLoading').hidden=true;}
    return;
  }
  const state=lastState,layout=previewLayout,portrait=layout.rotation%180===0;
  // Keep the last complete device frame while the next layout is in flight.
  // Never substitute browser line wrapping or mix two layout revisions.
  if(!previewMatchesDocument(layout))return;
  if(['font','lineSpacing','letterSpacing','rotation','marginLeft','marginRight','mirror','textAlign','pageMode'].some(key=>layout[key]!==undefined&&layout[key]!==state[key]))return;
  const width=portrait?800:1280,height=portrait?1280:800,vh=layout.height??height-200;
  const remaining=previewCountdown.remaining(now);
  const motion=online?previewMotion.position(now):(state.scroll||0);
  const scroll=Math.floor(state.pageMode===1||state.loop?motion:Math.min(motion,layout.endScroll??Infinity));
  const signature=[state.revision,layout.revision,scroll,remaining?Math.floor(now/16):0,deviceSettings?.asleep,deviceSettings?.revision,screenTelemetry?.footerText,Math.floor(Date.now()/1000),uiLanguage].join(':');
  if(signature===previewDrawSignature)return;previewDrawSignature=signature;
  const resized=previewCanvas.width!==width||previewCanvas.height!==height,wasHidden=previewCanvas.hidden;
  if(resized){previewCanvas.width=width;previewCanvas.height=height;}
  previewCanvas.hidden=false;$('previewLoading').hidden=true;
  if(resized||wasHidden){$('preview').style.aspectRatio=`${width} / ${height}`;fitDevicePreview();}
  // Reuse the drawing surface between frames; resizing it on every scroll
  // reallocates the full bitmap and forces a layout read at animation speed.
  const ctx=previewContext;ctx.setTransform(1,0,0,1,0,0);ctx.globalAlpha=1;ctx.fillStyle='#000';ctx.fillRect(0,0,width,height);
  const color='#'+Number(state.textColor).toString(16).padStart(6,'0');
  const left=layout.marginLeft,viewWidth=layout.width;
  drawScreenBars(state,width,height);
  if(!layout.text){
    const cardWidth=width>800?720:width-80,innerWidth=cardWidth-60;
    const heading=uiLanguage==='en'?'Ready for your script':'等待发送稿件';
    const address=state.ip||location.hostname,ap=deviceSettings?.activeApName||'Teleprompter-P4';
    const body=uiLanguage==='en'?`Open in your browser\nhttp://${address}\n\nDevice hotspot\n${ap}\n\nSend a script from the controller to begin`:`浏览器打开\nhttp://${address}\n\n设备热点\n${ap}\n\n在控制台发送稿件即可开始提词`;
    ctx.font='30px DeviceReader';
    const rows=[];
    for(const paragraph of body.split('\n')){
      let row='',rowWidth=0;
      for(const char of paragraph){const advance=Math.round(ctx.measureText(char).width);if(rowWidth+advance>innerWidth&&row){rows.push(row);row='';rowWidth=0;}row+=char;rowWidth+=advance;}
      rows.push(row);
    }
    const cardHeight=60+43+20+rows.length*49-6,x=Math.floor((width-cardWidth)/2),y=Math.floor((height-cardHeight)/2);
    ctx.fillStyle='#101b26';ctx.strokeStyle='#32495c';ctx.lineWidth=2;ctx.beginPath();ctx.roundRect(x,y,cardWidth,cardHeight,24);ctx.fill();ctx.stroke();
    readerText(heading,width/2,y+30+35,30,'#cbef9c',0,true);
    rows.forEach((row,index)=>readerText(row,width/2,y+30+43+20+index*49+35,30,'#e8eff5',0,true));
    return;
  }
  const viewportTop=layout.viewportTop??90,textTop=layout.textTop??Math.floor(vh/4);
  ctx.save();ctx.beginPath();ctx.rect(left,viewportTop,viewWidth,vh);ctx.clip();ctx.translate(left,viewportTop);
  const bandHeight=layout.bandHeight??Math.min(vh-8,layout.lineHeight+32);
  const bandStart=Math.max(0,Math.min(vh-bandHeight,Math.floor(vh/4)-16));
  const bandTop=layout.bandTop??(state.mirror&2?vh-bandStart-bandHeight:bandStart);
  if(remaining>0){
    ctx.save();ctx.translate(viewWidth/2,height/2-viewportTop);if(state.mirror&2)ctx.scale(1,-1);
    ctx.fillStyle=color;ctx.beginPath();ctx.arc(0,0,145,0,Math.PI*2);ctx.fill();
    ctx.strokeStyle=color;ctx.lineWidth=3;ctx.globalAlpha=.3;ctx.beginPath();ctx.arc(0,0,151,0,Math.PI*2);ctx.stroke();ctx.globalAlpha=1;
    ctx.lineWidth=4;ctx.lineCap='round';ctx.beginPath();ctx.arc(0,0,155,-Math.PI/2,-Math.PI/2+Math.PI*2*remaining/(state.countdown*1000));ctx.stroke();
    const age=(1000-remaining%1000)%1000,ease=1-Math.pow(1-Math.min(220,age)/220,2);
    const digitSize=Math.floor(Math.max(96,Math.min(150,state.font*1.5))*(.88+.12*ease));
    const rgb=Number(state.textColor),luma=((rgb>>16)&255)*299+((rgb>>8)&255)*587+(rgb&255)*114;
    const ink=luma>128000?'#10171c':'#fff';ctx.globalAlpha=(80+175*ease)/255;
    const digitHeight=Math.floor(digitSize*1.448),digitBase=Math.floor(digitSize*.288);
    readerText(String(Math.ceil(remaining/1000)),0,-Math.floor(digitHeight/2)+digitHeight-digitBase,digitSize,ink,0,true);
    ctx.restore();
  }else{
    ctx.save();if(state.mirror&2){ctx.translate(0,vh);ctx.scale(1,-1);}
    ctx.font=`${layout.font}px DeviceReader`;ctx.fontKerning='none';ctx.textBaseline='alphabetic';
    const lineStep=layout.lineHeight+layout.lineSpacing;
    const paged=state.pageMode===1;
    const firstRow=paged?state.pageIndex*layout.pageRows:Math.max(0,Math.ceil((scroll-textTop-layout.lineHeight)/lineStep));
    const rowCount=layout.playbackRows??layout.rows.length;
    const lastVisible=Math.floor((scroll-textTop+vh)/lineStep);
    const lastRow=paged?Math.min(rowCount-1,firstRow+layout.pageRows-1):state.loop&&rowCount?lastVisible:Math.min(rowCount-1,lastVisible);
    for(let index=firstRow;index<=lastRow;++index){
      const top=textTop-scroll+index*lineStep;
      drawReaderRow(layout,state,index%rowCount,top,viewWidth,color);
    }ctx.restore();
    if(!paged&&state.centerHighlight){ctx.fillStyle='#0008';ctx.fillRect(0,0,viewWidth,bandTop);ctx.fillRect(0,bandTop+bandHeight,viewWidth,vh-bandTop-bandHeight);}
    if(!paged&&state.sightGuide){ctx.strokeStyle='#cbef9c';ctx.lineWidth=2;ctx.strokeRect(1,bandTop+1,viewWidth-2,bandHeight-2);}
  }
  ctx.restore();
}
function previewLifecycle(){
  const action=lifecycleIntent?.action;
  if(action==='shutdown'||lastState?.lifecycle==='shutting_down')return online?'正在关机…':'设备已关机';
  if(!online)return action==='restart'?'正在重新启动…':'设备连接已断开，正在等待连接…';
  const phase=lastState?.lifecycle;
  if(phase==='asleep'||deviceSettings?.asleep)return '屏幕已休眠';
  if(phase==='restarting'||action==='restart')return '正在重新启动…';
  if(phase==='sleeping'||action==='standby')return '正在进入休眠…';
  if(phase==='waking'||action==='wake')return '正在唤醒屏幕…';
  if(phase==='updating')return '正在更新固件…';
  if(phase==='starting')return '正在启动…';
  return '';
}
requestAnimationFrame(drawDevicePreview);
