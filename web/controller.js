'use strict';
// Touch browsers can retain :hover after a tap. Only actual mouse input may
// enable hover styling; keyboard focus and persistent selection stay separate.
document.documentElement.dataset.pointer=matchMedia('(hover:hover) and (pointer:fine)').matches?'mouse':'touch';
for(const event of ['pointerdown','pointermove'])document.addEventListener(event,e=>{
  if(e.pointerType)document.documentElement.dataset.pointer=e.pointerType;
},{capture:true,passive:true});
const $=id=>document.getElementById(id),encoder=new TextEncoder(),decoder=new TextDecoder();
// Keyboard resizing and browser panning must not leave the editor at a stale height.
let viewportFrame=0,viewportBaseline=innerHeight,viewportWidth=innerWidth;
function updateMobileViewport(){
  const root=document.documentElement,view=window.visualViewport;
  const focused=document.activeElement?.matches('textarea,input:not([type=range]):not([type=checkbox]):not([type=color]),[contenteditable=true]');
  if(Math.abs(innerWidth-viewportWidth)>80){viewportBaseline=innerHeight;viewportWidth=innerWidth;}
  if(!focused)viewportBaseline=Math.max(innerHeight,view?.height||0);
  else viewportBaseline=Math.max(viewportBaseline,innerHeight);
  const zoomed=view&&Math.abs(view.scale-1)>.05;
  const visible=zoomed?innerHeight:(focused?Math.min(innerHeight,view?.height||innerHeight):innerHeight);
  const keyboard=!!focused&&!zoomed&&matchMedia('(pointer:coarse)').matches&&(viewportBaseline-visible>120||visible<540);
  root.style.setProperty('--mobile-viewport-height',Math.round(visible)+'px');
  root.style.setProperty('--visual-viewport-top',Math.round(view?.offsetTop||0)+'px');
  root.dataset.keyboard=keyboard?'open':'closed';
}
function scheduleViewportUpdate(){if(viewportFrame)return;viewportFrame=requestAnimationFrame(()=>{viewportFrame=0;updateMobileViewport();});}
updateMobileViewport();
for(const event of ['resize','orientationchange','pageshow'])window.addEventListener(event,scheduleViewportUpdate);
for(const event of ['resize','scroll'])window.visualViewport?.addEventListener(event,scheduleViewportUpdate);
document.addEventListener('visibilitychange',scheduleViewportUpdate);
for(const event of ['focusin','focusout'])document.addEventListener(event,()=>{
  scheduleViewportUpdate();for(const delay of [100,300,600])setTimeout(scheduleViewportUpdate,delay);
});
const uuid=n=>`7d2a000${n}-4b7c-4d40-9e22-2f0b8d0c3a11`;
const nativeApp=window.teleprompterNative||null;
const bluetoothApi=()=>nativeApp?.bluetooth||navigator.bluetooth;
let nativeForeground=true;
let mode=document.documentElement.dataset.defaultTransport||'lan',online=false,busy=false,wifiLoaded=false,settingsPollAt=0;
let wifiState=null,wifiPending=false,wifiRequestGeneration=0;
let wifiPollAt=0;
let bleDevice,bleStatus,bleControl,bleScript,lastState=null,lastSent=null,queue=Promise.resolve(),epoch=0,polling=false,draftTimer;
let bleSettings,bleLibrary,deviceSettings=null;
let bleConnecting=false,bleManualDisconnect=false,bleReconnectTimer;
let bleChoosing=false,bleChoiceGeneration=0,bleAvailability=null,bleUnsupportedReason='';
let lifecycleIntent=null;
let deviceResponseAt=0;
let controllerOccupied=false;
let connectionNoticeReadyAt=performance.now()+1500,connectionNoticeSince=0;
let bleGattQueue=Promise.resolve();
let bleLibraryQueue=Promise.resolve();
let autoLanguageSaving=false;
let deviceLanguageSynced='';
const deviceSettingsEdits=new Map();
const hosted=document.documentElement.dataset.hosted==='true';
/* MOTION_JS */
const clientId=Array.from(crypto.getRandomValues(new Uint8Array(12)),b=>b.toString(16).padStart(2,'0')).join('');
const typographyDefaults={font:90,letterSpacing:8,lineSpacing:20,countdown:5};
const readingDefaults={loop:true,autoNext:false,bold:false,textAlign:0,marginLeft:8,marginRight:8,textColor:0xffffff,mirror:0,sightGuide:true,centerHighlight:true};
let countdownDeadline=0;
let playbackPending=false;
let writeGeneration=0;
const pendingControls=new Map(),editingControls=new Set();
const liveControls=new Map();let liveControlTimer,liveControlSending=false;
let liveSocket=null,liveSocketRetry,liveRequestId=0,liveReceivedAt=0;
const liveRequests=new Map();
function connectLiveSocket(){
  if(mode!=='lan'||hosted||document.hidden||liveSocket&&[WebSocket.OPEN,WebSocket.CONNECTING].includes(liveSocket.readyState))return;
  clearTimeout(liveSocketRetry);
  const socket=new WebSocket(`${location.protocol==='https:'?'wss:':'ws:'}//${location.host}/api/live?client=${clientId}`);liveSocket=socket;
  socket.onmessage=event=>{
    if(socket!==liveSocket)return;
    let message;try{message=JSON.parse(event.data);}catch{return;}
    liveReceivedAt=performance.now();
    if(message.heartbeat)return;
    if(message.id){
      const pending=liveRequests.get(message.id);if(!pending)return;
      liveRequests.delete(message.id);clearTimeout(pending.timer);
      if(message.error)pending.reject(Error(tr('设备拒绝了请求')));else pending.resolve(message.state);
    }else if(!busy&&!pendingControls.size&&!liveControls.size)render(message);
  };
  socket.onclose=()=>{
    if(socket!==liveSocket)return;liveSocket=null;
    for(const pending of liveRequests.values()){clearTimeout(pending.timer);pending.reject(Error(tr('设备尚未连接')));}liveRequests.clear();
    if(mode==='lan')liveSocketRetry=setTimeout(connectLiveSocket,2000);
  };
  socket.onerror=()=>{};
}
function socketControl(values){
  const id=++liveRequestId;
  return new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>{liveRequests.delete(id);reject(Error(tr('设备响应超时，请检查连接后重试。')));liveSocket?.close();},10000);
    liveRequests.set(id,{resolve,reject,timer});
    liveSocket.send(JSON.stringify({id,query:new URLSearchParams(values).toString()}));
  });
}
const controlKeys=['speed','font','rotation','brightness','letterSpacing','lineSpacing','countdown','showInfo',...Object.keys(readingDefaults)];
function syncControl(key,value){
  const el=$(key);if(!el)return;
  if(el.type==='checkbox')el.checked=key==='rotation'?Number(value)%180===0:key==='mirror'?!!(Number(value)&2):!!Number(value);
  else if(el.type==='color')el.value='#'+Number(value).toString(16).padStart(6,'0');
  else el.value=value;
  if(el.type==='range')updateRangeAppearance(el);
  if($(key+'Number'))$(key+'Number').value=el.value;
  if($(key+'Value'))$(key+'Value').textContent=el.value;
  if(key==='textAlign')for(const button of document.querySelectorAll('[data-align]'))button.setAttribute('aria-pressed',String(Number(button.dataset.align)===Number(value)));
}
function updateRangeAppearance(input){
  const min=Number(input.min||0),max=Number(input.max||100),value=Number(input.value);
  input.style.setProperty('--range-progress',Math.max(0,Math.min(100,(value-min)/(max-min||1)*100))+'%');
}
document.addEventListener('input',event=>{
  if(event.target.matches('input[type=range]'))updateRangeAppearance(event.target);
  else if(event.target.id.endsWith('Number')){
    const range=$(event.target.id.slice(0,-6));if(range?.type==='range')updateRangeAppearance(range);
  }
});
function editing(key){return editingControls.has(key)||pendingControls.has(key)||liveControls.has(key);}
let messageTimer;
// Keyed notifications update in place; polling must not reopen expired notices.
const statusToasts=new WeakMap();
function statusToast(element,key,duration=8000){
  const previous=statusToasts.get(element);
  if(previous?.key===key)return;
  clearTimeout(previous?.timer);
  const state={key,timer:0};statusToasts.set(element,state);
  if(!key){motionHide(element);return;}
  motionShow(element);
  if(duration)state.timer=setTimeout(()=>motionHide(element),duration);
}
function userError(error){
  const text=String(error?.message??error??'');
  if(text.includes('factory_reset_busy'))return tr('设备正在恢复出厂设置，请稍候');
  if(error instanceof Error)console.warn('Controller operation failed',error.name,error.message);
  if(error?.status===423||text.includes('controller_busy'))return tr('网页控制名额已占用，请关闭原网页，约 20 秒后重试；蓝牙可独立连接。');
  if(text==='firmware_busy')return tr('设备正在更新，请等待完成');
  if(text.includes('network_address_forbidden'))return tr('请通过热点或局域网修改域名和 IP；蓝牙可修改 Wi-Fi 名称和密码');
  if(text.includes('invalid_network_settings'))return tr('网络地址无效，请检查 IP、网关、子网掩码和本地域名');
  if(text.includes('invalid_wifi_settings'))return tr('Wi-Fi 配置无效，请检查名称和密码');
  if(error?.status===429||error?.status===503)return tr('设备暂忙，请稍后重试');
  if(error?.status>=500)return tr('设备处理请求失败，请稍后重试');
  if(error?.name==='AbortError'&&/connection changed/i.test(text)||/preview_superseded/i.test(text))return null;
  if(error?.name==='TimeoutError'||/timed?\s*out|timeout|signal is aborted|operation was aborted|aborted without reason/i.test(text))return tr('设备响应超时，请检查连接后重试。');
  if(/gatt|bluetooth.*disconnect|disconnect.*bluetooth/i.test(text))return tr('蓝牙连接已断开，请重新连接设备。');
  if(error?.name==='NetworkError'||/failed to fetch|fetch failed|load failed|network\s*(error|request failed)|networkerror/i.test(text))return tr('无法连接设备，请检查网络或蓝牙连接后重试。');
  if(error?.name==='AbortError')return tr('请求已中止，请重试。');
  if(error?.name==='SyntaxError'||/unexpected token|unexpected end|json.*parse|invalid json/i.test(text))return tr('设备返回的数据不完整，请重试。');
  if(/[\u3400-\u9fff]/.test(text)||Object.values(translations).includes(text))return text;
  return tr('操作未完成，请重试；若仍失败，请重新连接设备。');
}
function message(text,error=false){
  if(error){text=userError(text);if(text===null)return;}
  const el=$('message');
  if(text&&!el.hidden&&el.textContent===text&&el.classList.contains('error')===error)return;
  clearTimeout(messageTimer);
  if(!text){motionHide(el);el.textContent='';return;}
  const changed=el.hidden||el.textContent!==text;
  el.textContent=text;el.className=error?'error':'';el.hidden=false;
  delete el.dataset.loading;
  if(changed)motionEnter(el);
  messageTimer=setTimeout(()=>{motionHide(el);el.textContent='';},error?8000:3000);
}
function badge(text,ready=online){
  const fresh=online&&performance.now()-deviceResponseAt<4000;
  const now=performance.now();
  if(fresh)connectionNoticeSince=0;else if(!connectionNoticeSince)connectionNoticeSince=now;
  const stableNotice=now>=connectionNoticeReadyAt&&now-connectionNoticeSince>=1200;
  const activityVisible=renderDeviceActivity();
  const state=fresh?tr('设备在线'):online?tr('等待设备响应'):tr('设备离线');
  $('connection').textContent=state;
  $('connectionWifi').toggleAttribute('hidden',mode==='ble');$('connectionBluetooth').toggleAttribute('hidden',mode!=='ble');
  $('connection').setAttribute('aria-label',(mode==='ble'?tr('蓝牙'):'Wi-Fi')+' · '+state);
  $('connectionBadge').dataset.state=fresh?'ready':online?'waiting':'offline';
  $('connectionBadge').dataset.transport=mode;
  $('connectionBadge').dataset.tooltip=tr('设备在线表示已收到提词器响应；通信连接成功不等于设备已响应。');
  $('deviceConnectionBadge').hidden=true;
  $('deviceConnection').textContent=state;
  // Connection requires a persistent in-page action, separate from transient toasts.
  if(!activityVisible&&mode==='ble'&&!online&&stableNotice)motionShow($('blePanel'));
  else motionHide($('blePanel'));
  $('networkNotice').dataset.state=online?'waiting':'offline';
  $('networkNoticeText').textContent=controllerOccupied?userError({status:423}):tr(online?'设备响应较慢，正在尝试重新连接。':mode==='ble'?'蓝牙连接已断开，请重新连接设备。':'无法连接提词器，请检查 Wi-Fi 和设备电源，正在自动重连。');
  statusToast($('networkNotice'),!activityVisible&&stableNotice&&!fresh&&!(mode==='ble'&&!online)?$('networkNoticeText').textContent:null);
  if(!fresh)setPreviewStatus(online?tr('等待设备响应'):tr('设备离线'),online?'loading':'offline');
}
// A request lock prevents duplicate commands without making an online control
// look disconnected. Native disabled behavior still blocks clicks and submits.
function setControlDisabled(element,unavailable,pending=false){
  const temporary=!unavailable&&!!pending;
  element.toggleAttribute('data-pending',temporary);
  const disabled=!!unavailable||!!pending;
  if(element.disabled!==disabled)element.disabled=disabled;
}
function buttons(){
  const updating=typeof firmwareLocked==='function'&&firmwareLocked();
  const locked=updating||busy||playbackPending||(typeof libraryActionPending!=='undefined'&&libraryActionPending);
  document.querySelectorAll('[data-device]').forEach(el=>{
    const key=el.dataset.controlKey||el.id;
    const continuous=el.matches('input[type=range],input[type=number],input[type=color]');
    const readingControl=controlKeys.some(k=>el.id===k||el.id===k+'Number'||el.dataset.controlKey===k);
    setControlDisabled(el,updating||!online,playbackPending||(locked&&!readingControl)||(!continuous&&pendingControls.has(key)));
  });
  setControlDisabled($('send'),!online||updating||encoder.encode($('script').value).length>8192,locked);
  setControlDisabled($('load'),!online||updating,locked);
  for(const id of ['tabLan','tabBle'])setControlDisabled($(id),false,busy);
  setControlDisabled($('bleConnect'),!!bluetoothProblem(),busy||bleChoosing);
  $('bleConnect').textContent=tr(bleChoosing?'等待蓝牙选择…':'连接蓝牙');
  badge('');
  $('bleConnect').hidden=online;
  if(typeof libraryState!=='undefined')setControlDisabled($('libraryNew'),!online||updating||!libraryState?.mounted,locked);
  if(typeof libraryState!=='undefined')updateNextDocument();
  if(typeof libraryView!=='undefined')updateLibraryToolbar();
  if(typeof libraryState!=='undefined')renderStorageNotice();
  if(typeof firmwareButtons==='function')firmwareButtons();
  if(countdownDeadline)for(const id of ['backward','forward'])setControlDisabled($(id),true);
  if(typeof playbackDocumentPending==='function'&&playbackDocumentPending()){
    document.querySelectorAll('#playPage [data-device],#playEdit').forEach(el=>setControlDisabled(el,true));
  }
  if(typeof renderPlaybackDocument==='function')renderPlaybackDocument();
  wifiButtons();
  renderScreenAction(locked,updating);
  if(typeof screenBarButtons==='function')screenBarButtons();
}
function screenIsAsleep(){
  return lastState?.lifecycle==='asleep'||(lastState?.lifecycle!=='ready'&&!!deviceSettings?.asleep);
}
function renderScreenAction(locked=false,updating=false){
  const button=$('deviceSleep');if(!button?.dataset.actionIcon)return;
  const asleep=screenIsAsleep();
  setActionIcon(button,asleep?'wake':'sleep',asleep?'唤醒':'息屏');
  const known=['ready','asleep'].includes(lastState?.lifecycle)||typeof deviceSettings?.asleep==='boolean';
  const pending=['standby','wake'].includes(lifecycleIntent?.action)&&performance.now()-lifecycleIntent.at<10000;
  setControlDisabled(button,!online||updating||!known,locked||pending);
}
function refreshLanguage(){
  applyLanguage(uiLanguage);count();renderPlayback();
  renderBarCapacity();renderFirmwareStatus();
  for(const button of document.querySelectorAll('.icon-button'))if(button.dataset.actionLabel)button.setAttribute('aria-label',tr(button.dataset.actionLabel));
  if(editingDocument)renderEditorHeading();
  if(libraryState)renderLibrary(libraryState);
  if(wifiState)renderWifi(wifiState);else wifiButtons();
  if(online)void syncDeviceLanguage();
}
async function syncDeviceLanguage(){
  const key=epoch+':'+deviceSettings?.boot+':'+uiLanguage;
  if(autoLanguageSaving||!online||firmwareLocked()||!deviceSettings||deviceLanguageSynced===key)return;
  if(deviceSettings.language===uiLanguage){deviceLanguageSynced=key;return;}
  autoLanguageSaving=true;
  const language=uiLanguage;
  try{await run(async()=>{await writeDeviceSettings({language});deviceLanguageSynced=key;renderDeviceSettings(await readDeviceSettings());});}
  catch{}finally{autoLanguageSaving=false;}
}
function preview(){
  const rotation=lastState?.rotation??90;
  const portrait=rotation%180===0;
  if(!$('previewCanvas')||$('previewCanvas').hidden)$('preview').style.aspectRatio=portrait?'800 / 1280':'1280 / 800';
  if(typeof fitDevicePreview==='function')fitDevicePreview();
  if(pageName==='play'&&online)void refreshPreviewLayout();
}

function count(){
  const text=$('script').value,n=encoder.encode(text).length;
  $('wordCount').textContent=Array.from(text.replace(/\s/g,'')).length+tr(' 字');
  $('count').textContent=documentSize(n)+' / '+documentSize(8192);$('count').className=n>8192?'error':'';
  buttons();preview();return n;
}
function saveDraft(){markDocumentDirty();}
async function confirmAction(title,text,{danger=false,items=[],delay=0,confirmLabel='',cancelLabel='',dismissible=true}={}){
  const dialog=$('confirmDialog');$('confirmTitle').textContent=title;$('confirmText').textContent=text;
  dialog.dataset.dismissible=String(dismissible);
  const button=dialog.querySelector('[value=confirm]');button.classList.toggle('danger-confirm',danger);
  const action=title.replace(/[？?].*$/,'');
  dialog.querySelector('[value=cancel]').textContent=cancelLabel||tr('取消');
  dialog.querySelector('[value=cancel]').hidden=!dismissible;
  dialog.querySelector('.confirm-items')?.remove();
  if(items.length){const list=document.createElement('ul');list.className='confirm-items';for(const text of items){const item=document.createElement('li');item.textContent=text;list.append(item);} $('confirmText').after(list);}
  const deadline=performance.now()+delay*1000;
  let timer;
  const update=()=>{const remaining=Math.max(0,Math.ceil((deadline-performance.now())/1000));button.disabled=remaining>0;button.textContent=(confirmLabel||(danger?action:tr('确定')))+(remaining?' ('+remaining+'s)':'');if(remaining)timer=setTimeout(update,100);};
  update();dialog.returnValue='';
  return new Promise(resolve=>{dialog.addEventListener('close',()=>{clearTimeout(timer);button.disabled=false;resolve(dialog.returnValue==='confirm'&&performance.now()>=deadline);},{once:true});dialog.showModal();if(delay&&dismissible)dialog.querySelector('[value=cancel]').focus();else button.focus();motionEnter(dialog,'dialog');});
}
let dialogClosing=false;
function closeConfirmDialog(value){
  if(value!=='confirm'&&$('confirmDialog').dataset.dismissible==='false')return;
  if(dialogClosing)return;dialogClosing=true;
  const dialog=$('confirmDialog');
  const done=()=>{dialogClosing=false;if(dialog.open)dialog.close(value);};
  const animation=motionAnimate(dialog,[{opacity:1,transform:'none'},{opacity:0,transform:'translateY(6px) scale(.99)'}],{duration:120});
  if(animation)animation.finished.then(done,done);else done();
}
$('confirmDialog').querySelector('form').addEventListener('submit',event=>{
  event.preventDefault();if(event.submitter?.disabled)return;closeConfirmDialog(event.submitter?.value||'cancel');
});
$('confirmDialog').addEventListener('cancel',event=>{event.preventDefault();closeConfirmDialog('cancel');});
const apiRequests=new Set();
let apiPageLeaving=false;
window.addEventListener('pagehide',()=>{
  apiPageLeaving=true;
  for(const abort of apiRequests)abort.abort(new DOMException('Connection changed','AbortError'));
});
window.addEventListener('pageshow',()=>{apiPageLeaving=false;});
async function api(path,options={}){
  if(apiPageLeaving)throw new DOMException('Connection changed','AbortError');
  const abort=new AbortController();let timedOut=false;
  const forwardAbort=()=>abort.abort(options.signal.reason);
  if(options.signal?.aborted)forwardAbort();
  else options.signal?.addEventListener('abort',forwardAbort,{once:true});
  apiRequests.add(abort);
  const timer=setTimeout(()=>{timedOut=true;abort.abort();},10000);
  try{
    const r=await fetch(path,{cache:'no-store',...options,signal:abort.signal});
    /* A response header is not completion: keep timeout/cancellation active
     * until the body arrives, including JSON returned by the SD library. */
    const body=await r.arrayBuffer();
    if(!r.ok){const error=Error(decoder.decode(body).slice(0,180)||tr('设备拒绝了请求'));error.status=r.status;throw error;}
    if(apiPageLeaving||abort.signal.aborted)throw abort.signal.reason||new DOMException('Connection changed','AbortError');
    return new Response(r.status===204||r.status===205?null:body,{status:r.status,statusText:r.statusText,headers:r.headers});
  }
  catch(error){if(apiPageLeaving)throw new DOMException('Connection changed','AbortError');if(timedOut)throw new DOMException('Request timeout','TimeoutError');throw error;}
  finally{clearTimeout(timer);apiRequests.delete(abort);options.signal?.removeEventListener('abort',forwardAbort);}
}
function render(state){
  if(lastState?.boot!==undefined && state.boot===lastState.boot && state.revision<lastState.revision)return;
  if(state.boot===lastState?.boot&&state.revision===lastState?.revision&&
    Number.isFinite(state.scrollSampleMs)&&Number.isFinite(lastState?.scrollSampleMs)&&
    ((state.scrollSampleMs-lastState.scrollSampleMs)|0)<0)return;
  const rebooted=lastState?.boot!==undefined&&state.boot!==lastState.boot;
  const scriptChanged=lastState?.scriptRevision!==undefined&&state.scriptRevision!==undefined&&state.scriptRevision!==lastState.scriptRevision;
  if(state.stopped!==undefined||rebooted||scriptChanged||state.playing||state.countdownRemainingMs>0)legacyStop=null;
  if(rebooted||scriptChanged)resetDevicePreview();
  if(rebooted){
    firmwareState=null;firmwarePollAt=0;
    if(pageName==='play'&&libraryState?.active){
      const active=libraryState.items.find(item=>item.title===libraryState.active);
      playbackRestoreHint={title:libraryState.active,id:active?.id,boot:state.boot};
    }
  }
  previewMotion.update({...state,loopPeriod:previewLayout&&previewLayout.revision===state.previewRevision?previewLayout.loopPeriod:undefined},performance.now());
  lastState=state;online=true;controllerOccupied=false;deviceResponseAt=performance.now();
  renderPlaybackBarVisibility();
  if(lifecycleIntent&&lifecycleIntent.boot!==undefined&&state.boot!==lifecycleIntent.boot)lifecycleIntent=null;
  if(['restarting','shutting_down'].includes(state.lifecycle))lifecycleIntent={action:state.lifecycle==='restarting'?'restart':'shutdown',boot:state.boot,at:performance.now()};
  if(lifecycleIntent&&((lifecycleIntent.action==='standby'&&state.lifecycle==='asleep')||(lifecycleIntent.action==='wake'&&state.lifecycle==='ready')))lifecycleIntent=null;
  previewReceivedAt=performance.now();
  countdownDeadline=state.countdownRemainingMs>0?performance.now()+state.countdownRemainingMs:0;
  renderPlayback();$('powerWarning').hidden=!state.powerRecovery;
  $('displayRetry').hidden=!state.displayRetryAvailable||mode==='ble';
  for(const key of ['speed','rotation'])if(!editing(key))syncControl(key,state[key]);
  for(const [key,fallback] of Object.entries(typographyDefaults)){
    if(!editing(key))syncControl(key,state[key]??fallback);
    $(key+'Value').textContent=$(key).value;
  }
  if(!editing('brightness'))syncControl('brightness',state.brightness??100);
  $('brightnessValue').textContent=$('brightness').value;
  $('speedValue').textContent=$('speed').value;buttons();
  for(const [key,fallback] of Object.entries(readingDefaults))if(!editing(key))syncControl(key,state[key]??fallback);
  $('textColorValue').textContent=$('textColor').value.toUpperCase();preview();
}
function setPreviewStatus(text,status='paused'){
  const badge=$('playState');
  if(badge.dataset.state!==status||!badge.querySelector('.preview-state-text')){
    const paths={paused:'<path d="M8 5v14M16 5v14"/>',stopped:'<rect x="6" y="6" width="12" height="12" rx="1"/>',sleep:'<path d="M20 14a8 8 0 0 1-10-10 8 8 0 1 0 10 10Z"/>',offline:'<path d="m4 4 16 16M8 8a7 7 0 0 1 8 8M3 9a14 14 0 0 1 2-2m4-2a14 14 0 0 1 12 4M7 13l2-1m4 0 4 1M11 17h2"/>',loading:'<path d="M20 12a8 8 0 1 1-8-8"/>',countdown:'<circle cx="12" cy="12" r="8"/><path d="M12 7v5l3 2"/>',playing:'<circle cx="12" cy="12" r="5"/>'};
    badge.dataset.state=status;
    badge.innerHTML='<svg class="preview-state-icon" viewBox="0 0 24 24" aria-hidden="true">'+(paths[status]||paths.paused)+'</svg><span class="preview-state-text"></span>';
  }
  const label=badge.querySelector('.preview-state-text');
  if(label.textContent!==text)label.textContent=text;
}
let legacyStop=null;
function playbackStopped(state=lastState){
  if(typeof state?.stopped==='boolean')return state.stopped;
  if(state?.playbackFinished)return true;
  return !!legacyStop&&legacyStop.epoch===epoch&&state?.boot===legacyStop.boot&&
    state?.scriptRevision===legacyStop.scriptRevision&&state?.rewindRevision===legacyStop.rewindRevision&&
    state?.revision>=legacyStop.revision&&!state?.playing&&!state?.countdownRemainingMs;
}
function renderPlayback(){
  renderDocumentStatus();
  badge('');
  if(!online){setPreviewStatus(tr('设备离线'),'offline');return;}
  const remaining=countdownDeadline?Math.max(0,Math.ceil((countdownDeadline-performance.now())/1000)):0;
  const stopped=playbackStopped();
  const text=deviceSettings?.asleep?tr('屏幕休眠'):remaining?remaining+tr(' 秒后播放'):countdownDeadline?tr('正在开始…'):lastState?.playing?tr('播放中'):stopped?tr('已停止'):tr('已暂停');
  const status=deviceSettings?.asleep?'sleep':countdownDeadline?'countdown':lastState?.playing?'playing':stopped?'stopped':'paused';
  setPreviewStatus(text,status);
  const active=!!(lastState?.playing||countdownDeadline),shuttle=lastState?.shuttle||0;
  $('play').setAttribute('aria-label',tr(active?'暂停':'播放'));
  $('play').dataset.tooltip=tr(active?'暂停':'播放');
  $('play').setAttribute('aria-pressed',String(active));
  // Manual and automatic stops share one state; an untouched script is neutral.
  $('rewind').setAttribute('aria-pressed',String(!active&&stopped));
  $('playIcon').toggleAttribute('hidden',active);$('pauseIcon').toggleAttribute('hidden',!active);
  for(const [id,sign] of [['backward',-1],['forward',1]]){
    $(id).setAttribute('aria-pressed',String(shuttle*sign>0));
    $(id+'Rate').textContent=shuttle*sign>0?'×'+Math.abs(shuttle):'';
  }
  badge((mode==='ble'?tr('蓝牙'):'Wi-Fi')+tr('已连接'));
}
async function readStatus(){return mode==='ble'?JSON.parse(decoder.decode(await bluetoothOperation(()=>bleStatus.readValue()))):(await api('/api/status?client='+clientId)).json();}
function bluetoothOperation(operation,timeoutMs=10000){
  const device=bleDevice,version=epoch;
  const pending=bleGattQueue.then(async()=>{
    if(device!==bleDevice||version!==epoch)throw new DOMException('Connection changed','AbortError');
    let timeout;
    try{
      const result=await Promise.race([operation(),new Promise((_,reject)=>{timeout=setTimeout(()=>{if(device===bleDevice&&version===epoch)device?.gatt.disconnect();reject(new DOMException('Bluetooth operation timeout','TimeoutError'));},timeoutMs);})]);
      if(device!==bleDevice||version!==epoch)throw new DOMException('Connection changed','AbortError');
      return result;
    }finally{clearTimeout(timeout);}
  });
  bleGattQueue=pending.catch(()=>{});
  return pending;
}
async function bleWrite(characteristic,data,withoutResponse=false){
  return bluetoothOperation(()=>{
    if(!bleDevice?.gatt.connected)throw new DOMException('Bluetooth disconnected','NetworkError');
    return withoutResponse?characteristic.writeValueWithoutResponse(data):characteristic.writeValueWithResponse(data);
  },15000);
}
function blePayloadSize(){
  const value=lastState?.bleWriteBytes;
  const payload=Number.isInteger(value)&&value>=17?Math.min(value,244):17;
  const nativeLimit=bleDevice?.maxWriteBytes;
  return nativeApp&&Number.isInteger(nativeLimit)?Math.min(payload,Math.max(1,nativeLimit-3)):payload;
}
function run(operation){
  const version=epoch;
  ++writeGeneration;
  const pending=queue.then(async()=>{
    if(version!==epoch)throw Error(tr('连接方式已切换，请重试'));
    if(!online)throw Error(tr('尚未连接设备，草稿已保留'));
    busy=true;buttons();
    try{const result=await operation();if(version!==epoch)return;const state=result&&typeof result.length==='number'?result:await readStatus();if(version===epoch)render(state);}finally{if(version===epoch){busy=false;buttons();}}
  });
  queue=pending.catch(e=>{if(version===epoch)message(e,true);});
  return pending;
}
function packet(op,value,wide=false){return value===undefined?Uint8Array.of(op):wide?Uint8Array.of(op,value&255,value>>8):Uint8Array.of(op,value);}
async function applyControl(values){
  if(values.marginLeft!==undefined)values={...values,marginRight:values.marginLeft};
  if(mode==='lan')return liveSocket?.readyState===WebSocket.OPEN?socketControl(values):(await api('/api/control?'+new URLSearchParams(values),{method:'POST'})).json();
  for(const [key,value] of Object.entries(values)){
    const ops={playing:1,speed:2,font:3,rotation:4,rewind:5,brightness:6,letterSpacing:7,lineSpacing:8,countdown:9,loop:11,textAlign:12,marginLeft:13,marginRight:14,textColor:15,mirror:16,sightGuide:17,centerHighlight:18,shuttle:19,autoNext:20,bold:21,showInfo:22};
    const number=Number(value);
    await bleWrite(bleControl,key==='textColor'?Uint8Array.of(15,number&255,(number>>8)&255,(number>>16)&255):packet(ops[key],key==='rewind'?undefined:number,['speed','rotation','marginLeft','marginRight'].includes(key)));
  }
  return readStatus();
}
function control(values){
  if(values.loop===true||Number(values.loop)===1)values={...values,autoNext:0};
  else if(values.autoNext===true||Number(values.autoNext)===1)values={...values,loop:0};
  const token={};
  for(const [key,value] of Object.entries(values))if(controlKeys.includes(key)){
    editingControls.delete(key);pendingControls.set(key,{token,value});syncControl(key,value);
  }
  return run(async()=>{
    const state=await applyControl(values);
    // Older firmware has no explicit stop field. Remember only an acknowledged
    // Stop for this session/document/rewind revision; never infer it from offset 0.
    if(state&&state.stopped===undefined){
      if(values.rewind&&!Number(values.playing)&&!values.shuttle&&!state.playing&&!state.countdownRemainingMs)
        legacyStop={epoch,boot:state.boot,scriptRevision:state.scriptRevision,rewindRevision:state.rewindRevision,revision:state.revision};
      else if(Number(values.playing)===1||values.shuttle)legacyStop=null;
    }
    for(const key of Object.keys(values))if(pendingControls.get(key)?.token===token)pendingControls.delete(key);
    if(state.countdownRemainingMs>0)message(tr('倒计时已开始，可按暂停取消'));
    return state;
  }).catch(()=>{}).finally(()=>{
    let released=false;
    for(const key of Object.keys(values))if(pendingControls.get(key)?.token===token){pendingControls.delete(key);released=true;}
    if(released)buttons();
  });
}
async function playbackAction(values){
  if(playbackPending||busy||libraryActionPending||!online)return;
  if(values.shuttle&&countdownDeadline)return;
  playbackPending=true;buttons();
  try{await control(values);}finally{playbackPending=false;buttons();}
}
function queueLiveControl(key,value){
  editingControls.add(key);liveControls.set(key,value);
  if(!liveControlTimer&&!liveControlSending)liveControlTimer=setTimeout(flushLiveControls,60);
}
async function flushLiveControls(){
  clearTimeout(liveControlTimer);liveControlTimer=null;
  if(liveControlSending||!liveControls.size)return;
  const values=Object.fromEntries(liveControls);liveControls.clear();liveControlSending=true;
  try{await control(values);}finally{
    liveControlSending=false;
    for(const key of liveControls.keys())editingControls.add(key);
    if(liveControls.size)liveControlTimer=setTimeout(flushLiveControls,0);
  }
}
let wifiCredentialsDirty=false,networkFormEpoch=-1,networkSaveQueue=Promise.resolve();
const networkEdits=new Set(),networkSaveTimers=new Map();
const networkFieldIds=['networkHostname','hotspotLink','lanLink'];
function networkConnectionKind(w=wifiState){
  if(mode!=='ble'){
    if(w?.accessMode==='hotspot'||w?.accessMode==='lan')return w.accessMode;
    if(location.hostname===(w?.apIp||'192.168.4.1'))return 'hotspot';
  }
  return w?.connected?'lan':'hotspot';
}
function parseNetworkIp(raw){
  const match=String(raw).trim().match(/^(?:http:\/\/)?((?:\d{1,3}\.){3}\d{1,3})\/?$/i);
  if(!match)return null;
  const parts=match[1].split('.');
  if(parts.some(n=>Number(n)>255||String(Number(n))!==n))return null;
  return {text:parts.join('.'),value:parts.reduce((n,p)=>(n*256+Number(p))>>>0,0)};
}
function networkHostValid(ip,mask=0xffffff00){
  const first=ip>>>24,host=(ip&~mask)>>>0;
  return first>0&&first!==127&&first<224&&(ip>>>16)!==0xa9fe&&host!==0&&host!==((~mask)>>>0);
}
function collectNetworkAddress(field){
  if(field==='networkHostname'){
    const hostname=$('networkHostname').value.trim().toLowerCase().replace(/^http:\/\//,'').replace(/\/$/,'').replace(/\.local$/,'');
    if(hostname&&(hostname.length>63||!/^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$/.test(hostname)))throw Error(tr('本地域名只能包含字母、数字和连字符，最多 63 位'));
    if($('networkDomainEnabled').checked&&!hostname)throw Error(tr('启用本地域名时请填写域名'));
    return {hostname,mdnsEnabled:$('networkDomainEnabled').checked};
  }
  const n=wifiState?.network,ap=parseNetworkIp(field==='hotspotLink'?$('hotspotLink').value:n?.apIp||wifiState?.apIp||'192.168.4.1');
  if(!ap||!networkHostValid(ap.value))throw Error(tr('请输入有效的热点 IPv4 地址'));
  if(field==='hotspotLink'){
    const leaseIp=parseNetworkIp(n?.staticIp?n.ip:wifiState?.ip||''),leaseMask=parseNetworkIp(n?.staticIp?n.netmask:wifiState?.lease?.netmask||'');
    if(leaseIp&&leaseMask){const common=(leaseMask.value&0xffffff00)>>>0;if((ap.value&common)===(leaseIp.value&common))throw Error(tr('热点与局域网不能使用重叠的网段'));}
    return {apIp:ap.text};
  }
  const lease=n?.staticIp?n:wifiState?.connected?wifiState?.lease:null;
  const ip=parseNetworkIp($('lanLink').value),mask=parseNetworkIp(lease?.netmask||''),gateway=parseNetworkIp(lease?.gateway||''),dns=parseNetworkIp(lease?.dns||'');
  if(!mask||!gateway||!dns||!mask.value||!gateway.value||!dns.value)throw Error(tr('尚未获取网络参数，请先连接 Wi-Fi 后再修改局域网 IP'));
  if(!ip)throw Error(tr('请输入有效的局域网 IPv4 地址'));
  const inverse=(~mask.value)>>>0;
  if(!mask.value||inverse<3||(inverse&(inverse+1))!==0)throw Error(tr('子网掩码必须连续，且至少保留两个可用主机地址'));
  if(!networkHostValid(ip.value,mask.value)||!networkHostValid(gateway.value,mask.value)||ip.value===gateway.value||((ip.value&mask.value)>>>0)!==((gateway.value&mask.value)>>>0))throw Error(tr('局域网 IP 和网关必须处于同一网段，且不能相同'));
  if(!networkHostValid(dns.value,0))throw Error(tr('请输入有效的 DNS 地址'));
  const common=(mask.value&0xffffff00)>>>0;
  if((ap.value&common)===(ip.value&common))throw Error(tr('热点与局域网不能使用重叠的网段'));
  return {staticIp:true,ip:ip.text};
}
function networkPrimaryUrl(w=wifiState){
  const host=networkConnectionKind(w)==='lan'?w?.ip:w?.apEnabled!==false?w?.apIp||'192.168.4.1':'';
  return host?'http://'+host+'/':'';
}
function renderNetworkDetails(){
  const w=online?wifiState:null,n=w?.network,supported=n?.supported===true;
  if(w&&networkFormEpoch!==epoch){
    networkFormEpoch=epoch;networkEdits.clear();wifiCredentialsDirty=false;wifiPending=false;
    networkSaveQueue=Promise.resolve();
    for(const timer of networkSaveTimers.values())clearTimeout(timer);networkSaveTimers.clear();
  }
  if(w){
    if(!networkEdits.has('hotspotLink'))$('hotspotLink').value=n?.apIp||w.apIp||'192.168.4.1';
    if(!networkEdits.has('lanLink'))$('lanLink').value=n?.staticIp?n.ip||'':w.ip||'';
    if(!networkEdits.has('networkHostname'))$('networkHostname').value=n?.hostname?n.hostname+'.local':'';
    if(!networkEdits.has('networkHostname'))$('networkDomainEnabled').checked=!!n?.mdnsEnabled;
  }
  const bluetooth=mode==='ble',hotspot=networkConnectionKind(w)==='hotspot';
  $('networkAddressFields').hidden=bluetooth;
  $('hotspotAddressField').hidden=!hotspot;$('lanAddressField').hidden=hotspot;
  $('networkCapability').hidden=bluetooth||!w||supported;
  const apply=$('networkApplyStatus');
  apply.hidden=!n?.applyFailed&&!n?.pending;
  apply.textContent=n?.applyFailed?tr('网络地址未能应用，已恢复原配置，请检查后重试。'):n?.pending?tr('正在应用网络地址，请稍候。'):'';
}
function renderWifi(w){
  wifiState=w;
  for(const button of document.querySelectorAll('[data-connection-mode]'))button.setAttribute('aria-pressed',String(button.dataset.connectionMode===w.connectionMode));
  if(!wifiLoaded){$('wifiSsid').value=w.ssid;wifiLoaded=true;}
  wifiButtons();
}
async function wifiRequest(action='status',values={}){
  if(mode==='ble'){
    if(!bleLibrary||!deviceSettings?.wifiConfigSupported)throw Error(tr('请更新设备固件以支持蓝牙配置 Wi-Fi'));
    return libraryRequest({op:'wifi',action,...values});
  }
  const body={...values};if(body.network)body.network=JSON.stringify(body.network);
  const options=action==='status'?undefined:{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({action,...body})};
  return (await api('/api/wifi',options)).json();
}
async function pollWifi(version,generation){
  if(wifiPending||(mode==='ble'&&(!bleLibrary||!deviceSettings?.wifiConfigSupported)))return;
  const now=performance.now();
  const interval=!wifiState||wifiState.status==='connecting'||wifiState.switching?2000:pageName==='settings'?10000:60000;
  if(wifiPollAt&&now-wifiPollAt<interval)return;
  wifiPollAt=now;
  const requestGeneration=wifiRequestGeneration;
  try{
    const w=await wifiRequest();
    if(version===epoch&&generation===writeGeneration&&online&&!wifiPending&&requestGeneration===wifiRequestGeneration)renderWifi(w);
  }catch{} // Keep the control connection; retry auxiliary Wi-Fi reads independently.
}
function wifiButtons(){
  const connected=!!wifiState?.connected;
  const supported=mode==='lan'||!!deviceSettings?.wifiConfigSupported;
  const unavailable=!online||firmwareLocked()||!supported||!wifiState;
  renderNetworkDetails();
  $('wifiSave').hidden=false;
  $('wifiForget').hidden=!(connected||wifiState?.configured);
  setControlDisabled($('wifiSave'),!online||firmwareLocked()||!supported||!wifiState,busy||wifiPending||wifiState?.status==='connecting');
  setControlDisabled($('wifiForget'),!online||firmwareLocked()||!supported||!wifiState,busy||wifiPending);
  for(const id of ['wifiSsid','wifiOpen'])setControlDisabled($(id),unavailable,busy||wifiPending);
  setControlDisabled($('wifiPassword'),unavailable||$('wifiOpen').checked,busy||wifiPending);
  for(const id of networkFieldIds)setControlDisabled($(id),unavailable||!wifiState?.network?.supported,busy||wifiPending||!!wifiState?.switching);
  setControlDisabled($('networkDomainEnabled'),unavailable||typeof wifiState?.network?.mdnsEnabled!=='boolean',busy||wifiPending||!!wifiState?.switching);
  setControlDisabled($('networkHostname'),unavailable||!wifiState?.network?.supported||!$('networkDomainEnabled').checked,busy||wifiPending||!!wifiState?.switching);
  setControlDisabled($('networkAuto'),unavailable||!wifiState?.network?.supported||!wifiState.network.staticIp,busy||wifiPending||!!wifiState?.switching);
  for(const button of document.querySelectorAll('[data-connection-mode]'))setControlDisabled(button,!online||firmwareLocked()||!supported||!wifiState?.connectionMode,busy||wifiPending||!!wifiState?.switching);
}
function switchMode(next){
  if(mode===next&&lastState)return;
  if(hosted&&next==='lan'){message(tr('局域网控制请另开设备屏幕上的 http://IP 地址。此 HTTPS 页面通过附近设备的蓝牙控制。'));return;}
  if(busy)return;++epoch;++writeGeneration;pendingControls.clear();editingControls.clear();mode=next;online=false;lastState=null;
  connectionNoticeReadyAt=performance.now()+1500;connectionNoticeSince=0;
  clearTimeout(liveSocketRetry);if(liveSocket){liveSocket.close();liveSocket=null;}
  deviceSettings=null;deviceSettingsEdits.clear();bleSettings=null;settingsPollAt=0;
  lifecycleIntent=null;firmwareState=null;firmwarePollAt=0;renderDeviceActivity();
  wifiState=null;wifiLoaded=false;wifiPollAt=0;++wifiRequestGeneration;
  liveControls.clear();clearTimeout(liveControlTimer);
  bleLibrary=null;libraryState=null;activeDocument=null;libraryPollAt=0;
  $('libraryList').replaceChildren();$('storageNotice').hidden=true;libraryStorageFailure='';
  libraryLoading(true);
  resetDevicePreview();
  $('tabLan').setAttribute('aria-selected',String(mode==='lan'));$('tabBle').setAttribute('aria-selected',String(mode==='ble'));
  $('blePanel').hidden=true;$('networkNotice').hidden=true;$('wifiDetails').hidden=false;
  setPreviewStatus(tr('待连接'),'offline');$('powerWarning').hidden=true;$('displayRetry').hidden=true;
  clearTimeout(bleReconnectTimer);
  if(bleDevice?.gatt.connected){bleManualDisconnect=true;bleDevice.gatt.disconnect();}
  if(mode==='ble'){
    badge(tr('蓝牙未连接'));let hint=tr('首次连接请选择设备，后续会尝试自动重连。');
    if(bluetoothProblem())hint=tr(bluetoothProblem());
    $('bleHint').textContent=hint;libraryLoading(false);
  }else badge(tr('正在连接…'));
  buttons();message('');poll();
  if(mode==='ble'){void probeBluetoothAvailability();tryRememberedBluetooth();}
  else connectLiveSocket();
}
$('tabLan').onclick=()=>switchMode('lan');$('tabBle').onclick=()=>switchMode('ble');
function invalidBluetoothDevice(){
  const error=new Error(tr('此设备不兼容当前提词器，请重新选择设备。'));
  error.name='IncompatibleDeviceError';return error;
}
function readBluetoothStatus(value){
  let state;try{state=JSON.parse(decoder.decode(value));}catch{throw invalidBluetoothDevice();}
  if(!state||!Number.isFinite(state.boot)||!Number.isFinite(state.revision)||!Number.isFinite(state.length)||typeof state.playing!=='boolean')throw invalidBluetoothDevice();
  return state;
}
async function connectBluetooth(device){
  if(bleConnecting||mode!=='ble')return;
  clearTimeout(bleReconnectTimer);
  bleConnecting=true;busy=true;online=false;++epoch;
  wifiPollAt=0;
  queue=Promise.resolve();bleGattQueue=Promise.resolve();bleLibraryQueue=Promise.resolve();polling=false;bleDevice=device;bleManualDisconnect=false;
  bleStatus=bleControl=bleScript=bleSettings=bleLibrary=null;
  const version=epoch,current=()=>version===epoch&&device===bleDevice&&mode==='ble'&&!bleManualDisconnect;
  badge(tr('正在连接蓝牙…'));buttons();
  try{
    device.ongattserverdisconnected=()=>{
      if(mode!=='ble'||bleDevice!==device||bleConnecting)return;
      ++epoch;online=false;queue=Promise.resolve();bleGattQueue=Promise.resolve();bleLibraryQueue=Promise.resolve();polling=false;
      bleStatus=bleControl=bleScript=bleSettings=bleLibrary=null;
      badge(tr('蓝牙已断开'));buttons();if(!bleManualDisconnect)scheduleBluetoothReconnect();
    };
    for(let attempt=0;attempt<3;attempt++){
      try{
        if(!current())return;
        const server=await bluetoothOperation(()=>device.gatt.connect());
        if(!server.connected)throw new DOMException('Disconnected during discovery','NetworkError');
        const service=await bluetoothOperation(()=>server.getPrimaryService(uuid(1)));
        const handles=[];
        for(let id=2;id<=6;id++){
          if(!current()||!server.connected)throw new DOMException('Disconnected during discovery','NetworkError');
          try{handles.push(await bluetoothOperation(()=>service.getCharacteristic(uuid(id))));}
          catch(error){if(id>=5&&error.name==='NotFoundError'&&server.connected)handles.push(null);else throw error;}
        }
        const state=readBluetoothStatus(await bluetoothOperation(()=>handles[0].readValue()));
        const settings=handles[3]?JSON.parse(decoder.decode(await bluetoothOperation(()=>handles[3].readValue()))):null;
        if(!current()||!server.connected)throw new DOMException('Disconnected during discovery','NetworkError');
        [bleStatus,bleControl,bleScript,bleSettings,bleLibrary]=handles;
        // ATT discovery returns a compact settings record. Load its omitted
        // fields before publishing capabilities or marking the connection ready.
        const completeSettings=settings?.settingsChunked?await readDeviceSettings():settings;
        if(!current()||!server.connected)throw new DOMException('Disconnected during discovery','NetworkError');
        if(completeSettings){renderDeviceSettings(completeSettings);settingsPollAt=performance.now();}
        render(state);bluetoothApi()?.rememberVerifiedDevice?.(device);queueMicrotask(()=>syncDeviceLanguage());return;
      }catch(error){
        if(!current())return;
        device.gatt.disconnect();
        if(error.name==='NotFoundError'||error.name==='IncompatibleDeviceError'){
          bluetoothApi()?.forgetDevice?.(device);
          bleManualDisconnect=true;clearTimeout(bleReconnectTimer);
          const hint=invalidBluetoothDevice().message;$('bleHint').textContent=hint;message(hint,true);return;
        }
        if(attempt===2||!['NetworkError','InvalidStateError','AbortError'].includes(error.name))throw error;
        badge(tr('蓝牙连接中断，正在重试…'));await new Promise(resolve=>setTimeout(resolve,400*(attempt+1)));
      }
    }
  }catch(e){if(current()){
    online=false;device.gatt.disconnect();badge(tr('蓝牙未连接'));
    if(nativeApp&&['NotAllowedError','InvalidStateError','NotSupportedError'].includes(e.name)){
      bleManualDisconnect=true;if(e.name==='NotSupportedError')bleUnsupportedReason=e.message;message(tr(e.message),true);
    }
    else message(tr('蓝牙连接暂未完成，正在自动重试。请保持设备靠近。'),true);
  }}
  finally{busy=false;bleConnecting=false;buttons();}
  if(current()&&!online)scheduleBluetoothReconnect();
}
function scheduleBluetoothReconnect(){
  clearTimeout(bleReconnectTimer);if(bleManualDisconnect||mode!=='ble'||!nativeForeground)return;
  bleReconnectTimer=setTimeout(async()=>{if(!online&&!bleConnecting){await connectBluetooth(bleDevice);if(!online)scheduleBluetoothReconnect();}},3000);
}
async function tryRememberedBluetooth(){
  if(!bluetoothApi()?.getDevices||bluetoothProblem()||bleManualDisconnect)return;
  try{
    const devices=await bluetoothApi().getDevices();
    let updateDevice=null;try{updateDevice=sessionStorage.getItem('teleprompter-update-device');sessionStorage.removeItem('teleprompter-update-device');}catch{}
    const device=devices.find(item=>item.id===updateDevice)||(devices.length===1?devices[0]:null);
    if(mode==='ble'&&!online&&!bleConnecting&&device){await connectBluetooth(device);if(!online)scheduleBluetoothReconnect();}
  }catch{}
}
function bluetoothProblem(){
  if(!nativeApp&&!isSecureContext)return '蓝牙需要 HTTPS 或本机 localhost。局域网 HTTP 页面请使用 Wi-Fi 控制。';
  if(typeof bluetoothApi()?.requestDevice!=='function')return '当前浏览器未提供 Web Bluetooth，请换用支持的 Chrome / Edge。';
  if(bleUnsupportedReason)return bleUnsupportedReason;
  if(nativeApp&&bleAvailability===false)return '蓝牙未开启，请先开启手机蓝牙。';
  if(bleAvailability===false)return '浏览器报告蓝牙不可用，请开启系统蓝牙并检查权限，或使用局域网控制。';
  return '';
}
async function probeBluetoothAvailability(){
  const api=bluetoothApi();
  if(typeof api?.getAvailability!=='function')return;
  try{
    const available=await api.getAvailability();
    if(api!==bluetoothApi()||mode!=='ble')return;
    bleAvailability=available;
    if(!online&&!bleChoosing)$('bleHint').textContent=tr(bluetoothProblem()||'首次连接请选择设备，后续会尝试自动重连。');
    buttons();
  }catch{/* Unknown availability still permits a direct, user-activated request. */}
}
function bluetoothAvailabilityChanged(event){
  bleAvailability=event.value;buttons();
  if(mode==='ble'&&!online)$('bleHint').textContent=tr(bluetoothProblem()||'首次连接请选择设备，后续会尝试自动重连。');
  if(nativeApp&&event.value&&nativeForeground&&!online&&bleDevice&&!bleManualDisconnect)scheduleBluetoothReconnect();
}
bluetoothApi()?.addEventListener?.('availabilitychanged',bluetoothAvailabilityChanged);
$('bleConnect').onclick=async()=>{
  if(bleChoosing||bleConnecting||busy)return;
  if(bluetoothProblem()){message(tr(bluetoothProblem()),true);return;}
  clearTimeout(bleReconnectTimer);
  const generation=++bleChoiceGeneration;
  bleChoosing=true;buttons();
  $('bleHint').textContent=tr(nativeApp?'请在设备列表中选择提词器。':'请在浏览器弹出的设备窗口中选择提词器。');
  const timeout=setTimeout(()=>{
    if(generation!==bleChoiceGeneration)return;
    ++bleChoiceGeneration;bleChoosing=false;buttons();
    const hint=tr(nativeApp?'未完成设备选择，请关闭选择窗口后重试。':'浏览器未返回蓝牙选择结果。如未出现设备窗口，请更换浏览器或使用局域网控制。');
    $('bleHint').textContent=hint;message(hint,true);
  },30000);
  try{
    // Call directly inside the click handler; awaiting probes here loses user activation.
    const device=await bluetoothApi().requestDevice({filters:[{services:[uuid(1)]}]});
    if(generation!==bleChoiceGeneration||mode!=='ble')return;
    if(!device?.gatt||typeof device.gatt.connect!=='function'){
      bleUnsupportedReason='当前浏览器未实现完整的蓝牙连接功能，请更换浏览器或使用局域网控制。';
      $('bleHint').textContent=tr(bleUnsupportedReason);message(tr(bleUnsupportedReason),true);return;
    }
    bleChoosing=false;clearTimeout(timeout);await connectBluetooth(device);
  }
  catch(e){
    if(generation!==bleChoiceGeneration)return;
    if(nativeApp&&['NotAllowedError','InvalidStateError','NotSupportedError'].includes(e.name)){
      if(e.name==='NotSupportedError')bleUnsupportedReason=e.message;
      const hint=tr(e.message);$('bleHint').textContent=hint;message(hint,true);return;
    }
    if(e.name==='NotFoundError'||/cancelled|canceled/i.test(e.message)){
      $('bleHint').textContent=tr('已取消选择，可重新连接。');message('');return;
    }
    if(e.name==='NotSupportedError')bleUnsupportedReason='当前浏览器未实现完整的蓝牙连接功能，请更换浏览器或使用局域网控制。';
    const hint=tr(bleUnsupportedReason||'无法打开蓝牙选择器，请检查蓝牙和浏览器权限。');
    $('bleHint').textContent=hint;message(hint,true);
  }finally{
    clearTimeout(timeout);
    if(generation===bleChoiceGeneration){bleChoosing=false;buttons();}
  }
};
$('play').onclick=()=>{if(lastState?.length===0){message(tr('屏幕还没有稿件，请先发送。'),true);return;}playbackAction({playing:lastState?.playing||lastState?.countdownRemainingMs?0:1});};
$('pause').onclick=()=>playbackAction({playing:0});$('rewind').onclick=()=>playbackAction({rewind:1});
for(const [id,direction] of [['backward',-1],['forward',1]])$(id).onclick=()=>{
  const rate=Number(lastState?.shuttle)||0;
  playbackAction({shuttle:direction*(rate*direction>0?Math.abs(rate)%8+1:1)});
};
$('speed').oninput=()=>{$('speedValue').textContent=$('speed').value;queueLiveControl('speed',$('speed').value);};$('speed').onchange=()=>{queueLiveControl('speed',$('speed').value);void flushLiveControls();};
for(const key of [...Object.keys(typographyDefaults),'marginLeft']){
  const range=$(key),number=$(key+'Number'),output=$(key+'Value');
  range.oninput=()=>{number.value=range.value;output.textContent=range.value;queueLiveControl(key,range.value);};
  range.onchange=()=>{queueLiveControl(key,range.value);void flushLiveControls();};
  number.oninput=()=>{if(number.validity.valid&&number.value!==''){range.value=number.value;output.textContent=range.value;preview();}};
  number.onchange=()=>{
    if(number.value===''||!number.validity.valid){message(tr('请输入 ')+range.min+'–'+range.max+tr(' 之间的整数'),true);number.reportValidity();return;}
    range.value=number.value;output.textContent=range.value;queueLiveControl(key,range.value);void flushLiveControls();
  };
}
$('rotation').onchange=()=>{preview();control({rotation:$('rotation').checked?0:90});};
$('brightness').oninput=()=>{$('brightnessValue').textContent=$('brightness').value;queueLiveControl('brightness',$('brightness').value);};
$('brightness').onchange=()=>{queueLiveControl('brightness',$('brightness').value);void flushLiveControls();};
for(const key of ['loop','sightGuide','centerHighlight'])$(key).onchange=()=>{preview();control({[key]:Number($(key).checked)});};
$('textAlign').onchange=()=>{preview();control({textAlign:$('textAlign').value});};
for(const button of document.querySelectorAll('[data-align]'))button.onclick=()=>{
  const value=Number(button.dataset.align);syncControl('textAlign',value);control({textAlign:value});
};
$('mirror').onchange=()=>{preview();control({mirror:$('mirror').checked?2:0});};
$('textColor').oninput=()=>{$('textColorValue').textContent=$('textColor').value.toUpperCase();queueLiveControl('textColor',parseInt($('textColor').value.slice(1),16));};
$('textColor').onchange=()=>{queueLiveControl('textColor',parseInt($('textColor').value.slice(1),16));void flushLiveControls();};
for(const key of controlKeys)for(const id of [key,key+'Number'])if($(id)){
  $(id).addEventListener('input',()=>editingControls.add(key));
  $(id).addEventListener('blur',()=>{
    if(!editingControls.has(key)||pendingControls.has(key))return;
    if($(id).validity.valid&&$(id).value!=='')editingControls.delete(key);
  });
}
$('displayRetry').onclick=()=>run(async()=>{render(await(await api('/api/display/retry',{method:'POST'})).json());message(tr('正在重新启动屏幕，请稍候。'));}).catch(()=>{});
let deviceActionConfirmPending=false;
async function deviceAction(action){
  if(busy||!online||deviceActionConfirmPending)return;
  if(action==='restart'||action==='shutdown'){
    const confirmationEpoch=epoch;deviceActionConfirmPending=true;
    try{
      let description=tr(action==='shutdown'?'软件关机后屏幕、Wi-Fi 和蓝牙控制将关闭。请按 P4 的 RST 或重新上电开机；已保存稿件和配置保留。':'设备会先显示重启提示。已保存的 SD 卡稿件与系统配置保留。');
      if(deviceSettings?.startupPortal)description+='\n'+tr('重启后请连接设备热点，重新选择连接方式。');
      const ok=await confirmAction(tr(action==='shutdown'?'关机？':'重启提词器？'),description,{danger:true});
      if(!ok||confirmationEpoch!==epoch||!online)return;
    }finally{deviceActionConfirmPending=false;}
  }
  const version=epoch;
  ++writeGeneration;busy=true;buttons();
  const operation=queue.then(async()=>{
    if(version!==epoch||!online)throw Error(tr('设备尚未连接'));
    if(mode==='ble')await bleWrite(bleControl,packet(10,{restart:1,standby:2,wake:3,shutdown:5}[action]));
    else await api('/api/device?action='+action,{method:'POST'});
    lifecycleIntent={action,boot:lastState?.boot,at:performance.now()};
    renderDeviceActivity();
    if(action==='standby'||action==='wake')message(action==='standby'?tr('屏幕正在进入休眠，可用“唤醒屏幕”恢复。'):tr('正在唤醒屏幕。'));
    if(action==='shutdown'){bleManualDisconnect=true;clearTimeout(bleReconnectTimer);}
    if(action==='restart'){online=false;badge(tr('设备正在重启'));setPreviewStatus(tr('重启中'),'loading');}
  });
  queue=operation.catch(e=>message(e.message,true)).finally(()=>{busy=false;buttons();});
  await queue;
}
$('deviceSleep').onclick=()=>{if(!$('deviceSleep').disabled)return deviceAction(screenIsAsleep()?'wake':'standby');};
$('deviceShutdown').onclick=()=>deviceAction('shutdown');
$('deviceRestart').onclick=()=>deviceAction('restart');
function renderDeviceSettings(settings){
  if(deviceSettings?.boot!==undefined && settings.boot===deviceSettings.boot && settings.revision<deviceSettings.revision)return;
  deviceSettings=settings;
  applyLibraryView(settings);
  $('autoShutdownSeconds').closest('label').hidden=!Number.isFinite(settings.autoShutdownSeconds);
  for(const key of ['autoSleepSeconds','autoShutdownSeconds','apName','bleName'])if(!deviceSettingsEdits.has(key))$(key).value=settings[key]??0;
  if(!deviceSettingsEdits.has('apPassword'))$('apOpen').checked=!!settings.apOpen;
  updateHotspotPasswordField();
  $('deviceSettingsStatus').hidden=!settings.restartRequired;
  $('deviceSettingsStatus').textContent=settings.restartRequired?tr('名称或密码已保存，重启后生效。'):'';
  $('connectionBadge').title=settings.webClients===undefined?'':tr('控制连接：网页 ')+settings.webClients+tr(' · 蓝牙 ')+settings.bleClients;
  renderPlayback();
  renderPlaybackBarVisibility();
  wifiButtons();
  renderScreenAction(busy,firmwareLocked());
  if(!bleConnecting)queueMicrotask(()=>syncDeviceLanguage());
}
async function readDeviceSettings(){
  if(mode==='lan')return(await api('/api/settings')).json();
  if(!bleSettings)throw Error(tr('设备固件暂不支持设备设置'));
  const settings=JSON.parse(decoder.decode(await bluetoothOperation(()=>bleSettings.readValue())));
  return settings.settingsChunked?libraryRequest({op:'deviceSettings'}):settings;
}
async function writeDeviceSettings(settings){
  const body=JSON.stringify(settings);
  if(mode==='lan')await api('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body});
  else{
    if(!bleSettings)throw Error(tr('设备固件暂不支持设备设置'));
    const bytes=encoder.encode(body);
    await bleWrite(bleSettings,packet(0,bytes.length,true));
    const payload=blePayloadSize();
    for(let offset=0;offset<bytes.length;offset+=payload){const chunk=bytes.slice(offset,offset+payload),frame=new Uint8Array(3+chunk.length);frame.set(packet(1,offset,true));frame.set(chunk,3);await bleWrite(bleSettings,frame);}
    await bleWrite(bleSettings,packet(2));
  }
}
function saveImmediateSetting(settings){
  return run(async()=>{await writeDeviceSettings(settings);renderDeviceSettings(await readDeviceSettings());message(tr('设置已保存'));}).catch(()=>{});
}
$('showInfo').onchange=()=>lastState?.showInfo===undefined?
  saveImmediateSetting({showInfo:$('showInfo').checked}):control({showInfo:Number($('showInfo').checked)});
function updateHotspotPasswordField(){
  const open=$('apOpen').checked;
  $('apPasswordField').hidden=open;
  $('apPassword').required=!open&&!!deviceSettings?.apOpen;
}
$('apOpen').onchange=()=>{
  $('apPassword').value='';
  if($('apOpen').checked)deviceSettingsEdits.set('apPassword','');
  else deviceSettingsEdits.delete('apPassword');
  updateHotspotPasswordField();
};
for(const key of ['autoSleepSeconds','autoShutdownSeconds','apName','bleName','apPassword'])$(key).addEventListener('input',()=>{
  if(key==='apPassword'&&!$(key).value&&!$('apOpen').checked)deviceSettingsEdits.delete(key);
  else deviceSettingsEdits.set(key,$(key).value);
});
$('deviceSettingsForm').onsubmit=event=>{
  event.preventDefault();
  const seconds=Number($('autoSleepSeconds').value),apName=$('apName').value,bleName=$('bleName').value;
  if(!$('apOpen').checked&&deviceSettings?.apOpen&&!$('apPassword').value){message(tr('请设置热点密码，或开启无需密码'),true);return;}
  if($('autoSleepSeconds').value===''||!Number.isInteger(seconds)||seconds<0||seconds>86400){message(tr('自动休眠请输入 0–86400 之间的整数秒数'),true);return;}
  const shutdown=Number($('autoShutdownSeconds').value);
  if($('autoShutdownSeconds').value===''||!Number.isInteger(shutdown)||shutdown<0||shutdown>86400){message(tr('自动关机请输入 0–86400 之间的整数秒数'),true);return;}
  if(!apName||!bleName||apName!==apName.trim()||bleName!==bleName.trim()||/[\x00-\x1f\x7f]/.test(apName+bleName)||encoder.encode(apName).length>32||encoder.encode(bleName).length>29){message(tr('名称不能为空或带首尾空格；热点最多 32 字节，蓝牙最多 29 字节'),true);return;}
  if($('apPassword').value&&!/^[\x20-\x7e]{8,63}$/.test($('apPassword').value)){message(tr('热点密码需为 8–63 位英文字母、数字或符号'),true);return;}
  const edits=new Map(deviceSettingsEdits);
  const settings=Object.fromEntries([...edits].map(([key,value])=>[key,['autoSleepSeconds','autoShutdownSeconds'].includes(key)?Number(value):value]));
  if(!edits.size){message(tr('设备设置没有改动'));return;}
  run(async()=>{
    await writeDeviceSettings(settings);
    for(const [key,value] of edits)if(deviceSettingsEdits.get(key)===value){deviceSettingsEdits.delete(key);if(key==='apPassword')$('apPassword').value='';}
    renderDeviceSettings(await readDeviceSettings());
    message(deviceSettings.restartRequired?tr('设置已保存。名称和热点密码重启后生效，其余设置立即生效。'):tr('设备设置已保存，自动休眠与自动关机立即生效。'));
  }).catch(()=>{});
};
$('import').onclick=()=>$('fileInput').click();
$('fileInput').onchange=async()=>{
  const file=$('fileInput').files[0];if(!file)return;
  try{
    if(file.size>32768)throw Error(tr('文件过大，请导入 8192 字节以内的 UTF-8 文本。'));
    const text=new TextDecoder('utf-8',{fatal:true}).decode(await file.arrayBuffer()).replace(/^\uFEFF/,'');
    if(encoder.encode(text).length>8192)throw Error(tr('稿件超过 8192 字节，请缩短后再导入。'));
    if(text.includes('\0'))throw Error(tr('请选择 UTF-8 文本文件，文件中不能包含空字符。'));
    if($('script').value&&$('script').value!==text&&!await confirmAction(tr('替换当前草稿？'),tr('导入文件会替换编辑框中的正文，保存后才会写入 SD 卡。'),{danger:true}))return;
    $('script').value=text;count();saveDraft();message(tr('已导入 ')+file.name);
  }catch(e){message(e,true);}finally{$('fileInput').value='';}
};
$('clear').onclick=async()=>{if(!$('script').value)return;if(await confirmAction(tr('清空本机草稿？'),tr('只清空当前编辑区，屏幕上的稿件保持不变。'),{danger:true})){$('script').value='';count();saveDraft();message(tr('本机草稿已清空'));}};
for(const id of ['wifiSsid','wifiPassword'])$(id).addEventListener('input',()=>{wifiCredentialsDirty=true;});
$('wifiOpen').onchange=()=>{wifiCredentialsDirty=true;wifiButtons();};
function queueNetworkSave(task){
  const version=epoch;
  const pending=networkSaveQueue.then(async()=>{
    if(version!==epoch||!online||firmwareLocked())return;
    try{await task(version);}catch(e){if(version===epoch){const reason=userError(e);if(reason)message(tr('Wi-Fi 设置未完成：')+reason,true);}}
  });
  networkSaveQueue=pending.catch(()=>{});return pending;
}
function networkValuesChanged(n){
  const w=wifiState,c=w?.network;
  if(!n)return false;
  return ('hostname' in n&&n.hostname!==(c?.hostname||''))||('mdnsEnabled' in n&&n.mdnsEnabled!==!!c?.mdnsEnabled)||('apIp' in n&&n.apIp!==(c?.apIp||w?.apIp||'192.168.4.1'))||
    ('staticIp' in n&&n.staticIp!==!!c?.staticIp)||('ip' in n&&n.ip!==(c?.staticIp?c.ip:w?.ip));
}
async function saveNetworkValues(values,fields=[]){
  const network=values.network,credentials='ssid' in values;
  if(network&&!networkValuesChanged(network)){
    for(const field of fields)networkEdits.delete(field);
    if(!credentials){wifiButtons();return true;}
    delete values.network;
  }
  const version=epoch,old=wifiState,route=networkConnectionKind(old);
  const originals=new Map(fields.map(field=>[field,$(field).value]));
  const originalDomainEnabled=$('networkDomainEnabled').checked;
  const addressChanged=!!values.network&&Object.keys(values.network).some(key=>!['hostname','mdnsEnabled'].includes(key));
  if(addressChanged&&!await confirmAction(tr('更新网络地址？'),tr('当前网络连接可能暂时中断。请记下新地址，保存后重新连接设备。')))return false;
  if(version!==epoch||!online||wifiPending||firmwareLocked()||wifiState?.switching)return false;
  if([...originals].some(([field,value])=>$(field).value!==value))return false;
  if(originals.has('networkHostname')&&$('networkDomainEnabled').checked!==originalDomainEnabled)return false;
  wifiPending=true;++wifiRequestGeneration;wifiButtons();
  try{
    const result=await wifiRequest('save',values);
    if(version!==epoch)return false;
    if(result.ok===false)throw Error(result.error||'wifi_settings_failed');
    for(const [field,value] of originals)if($(field).value===value&&(field!=='networkHostname'||$('networkDomainEnabled').checked===originalDomainEnabled))networkEdits.delete(field);
    if(credentials){wifiCredentialsDirty=false;$('wifiPassword').value='';}
    $('networkReconnect').hidden=true;renderWifi(result);
    if(addressChanged&&mode==='lan'){
      const host=networkPrimaryUrl({...result,accessMode:route,apIp:values.network.apIp||result.apIp,ip:values.network.ip||result.ip});
      if(host){$('networkReconnect').hidden=false;$('networkReconnectLink').textContent=host;$('networkReconnectLink').href=host;}
    }
    message(tr('设置已保存'));return true;
  }finally{if(version===epoch){wifiPending=false;wifiButtons();}}
}
for(const id of networkFieldIds){
  $(id).addEventListener('input',()=>{networkEdits.add(id);});
  $(id).addEventListener('change',()=>{
    clearTimeout(networkSaveTimers.get(id));
    const version=epoch;
    networkSaveTimers.set(id,setTimeout(()=>{
      networkSaveTimers.delete(id);
      if(version!==epoch)return;
      void queueNetworkSave(async()=>{if(networkEdits.has(id))await saveNetworkValues({network:collectNetworkAddress(id)},[id]);});
    },200));
  });
}
$('networkDomainEnabled').onchange=async()=>{
  const version=epoch;
  const enabled=$('networkDomainEnabled').checked;
  networkEdits.add('networkHostname');wifiButtons();
  if(enabled&&!await confirmAction(tr('启用本地域名？'),tr('部分手机或网络不支持本地域名，可能无法访问。遇到这种情况，请使用设备显示的 IP 地址。'))){
    if(version!==epoch)return;
    $('networkDomainEnabled').checked=false;networkEdits.delete('networkHostname');renderNetworkDetails();wifiButtons();return;
  }
  if(version!==epoch||$('networkDomainEnabled').checked!==enabled)return;
  void queueNetworkSave(()=>saveNetworkValues({network:collectNetworkAddress('networkHostname')},['networkHostname']));
};
$('networkAuto').onclick=()=>{
  if($('networkAuto').disabled)return;
  clearTimeout(networkSaveTimers.get('lanLink'));networkSaveTimers.delete('lanLink');
  void queueNetworkSave(()=>saveNetworkValues({network:{staticIp:false}},['lanLink']));
};
$('wifiForm').onsubmit=event=>{
  event.preventDefault();if($('wifiSave').disabled||$('wifiSave').hidden)return;
  for(const timer of networkSaveTimers.values())clearTimeout(timer);networkSaveTimers.clear();
  void queueNetworkSave(async version=>{
    // The domain is independent: an invalid IP must not block its save.
    if(networkEdits.has('networkHostname')){
      try{await saveNetworkValues({network:collectNetworkAddress('networkHostname')},['networkHostname']);}
      catch(e){message(e,true);}
    }
    if(version!==epoch||!online)return;
    const values={},fields=[],id=networkConnectionKind()==='hotspot'?'hotspotLink':'lanLink';
    if(wifiState?.network?.supported&&networkEdits.has(id)){values.network=collectNetworkAddress(id);fields.push(id);}
    if(wifiCredentialsDirty||!wifiState?.network?.supported){
      const ssid=$('wifiSsid').value,password=$('wifiOpen').checked?'':$('wifiPassword').value;
      if(!ssid||encoder.encode(ssid).length>32)throw Error(tr('Wi-Fi 名称需要 1–32 字节'));
      if(!$('wifiOpen').checked&&!/^[\x20-\x7e]{8,63}$|^[0-9a-fA-F]{64}$/.test(password))throw Error(tr('请输入 8–63 位 Wi-Fi 密码；无密码网络请勾选相应选项'));
      Object.assign(values,{ssid,password});
    }
    if(Object.keys(values).length)await saveNetworkValues(values,fields);
  });
};
$('wifiForget').onclick=async()=>{
  if($('wifiForget').disabled||$('wifiForget').hidden)return;
  if(!await confirmAction(tr('忘记路由器 Wi-Fi？'),tr('设备将断开路由器并恢复 ')+(deviceSettings?.activeApName||'Teleprompter-P4')+tr(' 热点。当前控制页面可能暂时断开。'),{danger:true}))return;
  if(!online||busy||wifiPending)return;
  wifiPending=true;++wifiRequestGeneration;wifiButtons();
  try{renderWifi(await wifiRequest('forget'));$('wifiSsid').value='';$('wifiPassword').value='';wifiCredentialsDirty=false;message(tr('已忘记路由器 Wi-Fi，可连接设备热点。'));}
  catch(e){if(mode==='ble')message(tr('Wi-Fi 设置未完成：')+userError(e),true);else message(tr('连接断开后，请连接 ')+(deviceSettings?.activeApName||'Teleprompter-P4')+tr(' 热点重新打开页面。'),true);}
  finally{wifiPending=false;wifiButtons();}
};
document.addEventListener('keydown',event=>{
  if(event.ctrlKey&&event.key==='Enter'){event.preventDefault();if(!$('send').disabled)$('send').click();}
  if(event.altKey&&event.code==='Space'){event.preventDefault();$('play').click();}
});
async function poll(){
  if(polling||busy||document.hidden||!nativeForeground||apiPageLeaving)return;const version=epoch,generation=writeGeneration;polling=true;
  try{
    if(mode==='lan'){
      if(liveSocket?.readyState!==WebSocket.OPEN||performance.now()-liveReceivedAt>5000){const state=await readStatus();if(version!==epoch||generation!==writeGeneration)return;render(state);}
      const settingsChanged=lastState?.settingsRevision!==undefined&&deviceSettings?.revision!==lastState.settingsRevision;
      const settingsInterval=lastState?.settingsRevision===undefined?10000:60000;
      if(!deviceSettings||settingsChanged||performance.now()-settingsPollAt>settingsInterval){
        settingsPollAt=performance.now();
        if(version===epoch&&generation===writeGeneration){try{const settings=await readDeviceSettings();if(version===epoch&&generation===writeGeneration)renderDeviceSettings(settings);}catch{}}
      }
    }else if(online&&performance.now()-deviceResponseAt>(lastState?.playing||countdownDeadline?500:1500)){
      const pending=queue.then(()=>version===epoch&&online?readStatus():null);queue=pending.catch(()=>{});
      const state=await pending;if(state&&version===epoch&&generation===writeGeneration)render(state);
      const settingsChanged=lastState?.settingsRevision!==undefined&&deviceSettings?.revision!==lastState.settingsRevision;
      if(bleSettings&&(settingsChanged||deviceSettings?.settingsChunked||performance.now()-settingsPollAt>(lastState?.settingsRevision===undefined?10000:60000)||!deviceSettings)&&version===epoch&&generation===writeGeneration){
        const settingsRead=queue.then(()=>readDeviceSettings());queue=settingsRead.catch(()=>{});const settings=await settingsRead;
        if(version===epoch&&generation===writeGeneration){
          renderDeviceSettings(settings);settingsPollAt=performance.now();
        }
      }
    }
    if(online&&version===epoch&&generation===writeGeneration){
      await pollWifi(version,generation);
      if(version!==epoch||generation!==writeGeneration)return;
      await pollFirmware();
      if(version!==epoch||generation!==writeGeneration)return;
      if(firmwareLocked())return;
      await syncPlaybackPresence();
      if(pageName==='play')await refreshPreviewLayout();
      if(pageName==='play')await refreshScreenTelemetry();
      await syncLibrary();
    }
  }catch(error){if(error?.status===423){controllerOccupied=true;online=false;}if(version===epoch&&generation===writeGeneration){
    // A fresh WebSocket status can arrive while the HTTP fallback is failing.
    if(mode==='lan'&&online&&performance.now()-deviceResponseAt<4000)return;
    if(mode==='ble'&&bleDevice?.gatt.connected&&performance.now()-deviceResponseAt<10000)return;
    online=false;badge(mode==='ble'?tr('蓝牙已断开'):tr('未连接 · 请检查网络'));setPreviewStatus(tr('待连接'),'offline');buttons();if(mode==='ble'&&!bleManualDisconnect){bleDevice?.gatt.disconnect();scheduleBluetoothReconnect();}
  }}
  finally{polling=false;}
}
new ResizeObserver(()=>preview()).observe($('preview'));
applyLanguage(uiLanguage);
if(hosted)$('tabLan').parentElement.hidden=true;
/* LIBRARY_JS */
/* PREVIEW_JS */
/* FIRMWARE_JS */
/* TOOLTIPS_JS */
let liveHeartbeatAt=0;
setInterval(()=>{
  if(!document.hidden&&liveSocket?.readyState===WebSocket.OPEN&&performance.now()-liveHeartbeatAt>3000){
    liveHeartbeatAt=performance.now();
    if(lastState?.presenceHeartbeat)void syncPlaybackPresence();
    else try{liveSocket.send(JSON.stringify({heartbeat:true}));}catch{}
  }
},1000);
document.addEventListener('visibilitychange',()=>{
  if(document.hidden){clearTimeout(liveSocketRetry);if(liveSocket){const old=liveSocket;liveSocket=null;old.close();}}
  else{connectLiveSocket();void poll();}
});
document.addEventListener('teleprompter-app-state',event=>{
  nativeForeground=!!event.detail.isActive;
  if(!nativeForeground){clearTimeout(bleReconnectTimer);return;}
  scheduleViewportUpdate();void probeBluetoothAvailability();void poll();
  if(mode==='ble'&&!online&&!bleConnecting&&!bleChoosing){
    if(bleDevice&&!bleManualDisconnect)scheduleBluetoothReconnect();else void tryRememberedBluetooth();
  }
});
document.addEventListener('teleprompter-app-back',event=>{
  const dialog=document.querySelector('dialog[open]');
  if(dialog){event.preventDefault();dialog.close('cancel');return;}
  if(pageName!=='home'){
    event.preventDefault();
    if(pageName==='play'&&matchMedia('(max-width:960px)').matches&&playbackSettings.dataset.open==='true'){setPlaybackSheet(false);return;}
    $(pageName==='edit'?'editBack':pageName==='play'?'playBack':'settingsBack').click();
  }
});
for(const button of document.querySelectorAll('[data-connection-mode]'))button.onclick=async()=>{
  const target=button.dataset.connectionMode;
  if(target===wifiState?.connectionMode||wifiPending||!online)return;
  if(!await confirmAction(tr('切换网络方式？'),tr('网页连接可能在 5 秒后断开，蓝牙保持连接。请从新的网络入口继续。')))return;
  wifiPending=true;wifiButtons();
  try{renderWifi(await wifiRequest('mode',{mode:target}));message(tr('正在切换网络方式，蓝牙连接不受影响。'));}
  catch(error){message(error,true);}finally{wifiPending=false;wifiButtons();}
};
count();switchMode(mode);setInterval(poll,500);setInterval(()=>{if(!document.hidden)renderPlayback();},100);
document.querySelectorAll('input[type=range]').forEach(updateRangeAppearance);
document.documentElement.dataset.ui='ready';
