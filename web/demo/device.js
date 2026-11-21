/* Review-only gallery. No device APIs, remote fonts, radio or firmware writes. */
'use strict';
const $=id=>document.getElementById(id);
const previewCanvas=$('readerCanvas'),previewContext=previewCanvas.getContext('2d');
const clone=value=>structuredClone(value),bytes=value=>new TextEncoder().encode(value).length;
let uiLanguage='zh-CN',online=true,previewReceivedAt=0,previewDrawSignature='',previewLayout=null,layoutCache=null;
let lastState={},demoScript='',deviceSettings={...screenBarDefaults};
let libraryState={items:[],active:''},screenTelemetry=null;
const tr=text=>uiLanguage==='en'?({'下一个':'Next','准备中':'Ready','播放中':'Playing','已暂停':'Paused','已停止':'Stopped'}[text]||text):text;
const guides=Object.keys(guide.states),reading=['PLAYING','PAUSED','STOPPED','FINISHED','COUNTDOWN','SLEEP_PENDING','ASLEEP'];
const order=['STARTUP','JOIN','WIFI_CONNECTING','LAN','READY','COUNTDOWN','PLAYING','PAUSED','STOPPED','FINISHED','SLEEP_PENDING','ASLEEP','RESTART','FACTORY_RESET','UPDATE_CHECKING','UPDATE_WRITING','UPDATE_RESTART','SHUTDOWN','OFF'];
const images={JOIN:'join',LAN:'lan',BLUETOOTH:'bluetooth',RECONNECT:'lan'};
let generation=0,animation=false,fontReady=false,scroll=0,lastFrame=0,sceneAt=performance.now(),lastSystemProgress=-1;
const reducedMotion=matchMedia('(prefers-reduced-motion: reduce)'),transitions=new Map();
let progressKey='',progressTarget=-1,exiting=false,lastGuideStage=null;
$('motion').checked=!reducedMotion.matches;
for(const [key,value] of Object.entries(guide.layout))document.documentElement.style.setProperty('--guide-'+key.replaceAll('_','-'),value+'px');
for(const [key,value] of Object.entries(systemSettings))document.documentElement.style.setProperty('--'+key.replaceAll('_','-'),value+(key==='content_width'?'%':'px'));
document.documentElement.style.setProperty('--spin-ms',motionSettings.spin_ms+'ms');
document.documentElement.style.setProperty('--pulse-ms',motionSettings.pulse_ms+'ms');
function motionEnabled(){return $('motion').checked&&!reducedMotion.matches;}
function stopTransition(el){const running=transitions.get(el);if(running){running.cancel();transitions.delete(el);}}
function animateElement(el,frames,duration,keepFinal=false,easing='cubic-bezier(.2,.7,.2,1)'){
 stopTransition(el);if(!motionEnabled())return;
 const running=el.animate(frames,{duration,easing,fill:'both'});
 transitions.set(el,running);running.onfinish=()=>{if(!keepFinal&&transitions.get(el)===running){transitions.delete(el);running.cancel();}};
}
function reveal(el,leave=false){
 stopTransition(el);const base=getComputedStyle(el).transform,transform=base==='none'?'':base+' ';
 animateElement(el,leave?[{opacity:1,transform:base},{opacity:0,transform:transform+`translateY(${motionSettings.lift_px}px)`}]:[{opacity:0,transform:transform+`translateY(${motionSettings.lift_px}px)`},{opacity:1,transform:base}],leave?motionSettings.exit_ms:motionSettings.enter_ms,leave);
}
function slideGuide(){
 animateElement($('guideContent'),[{transform:`translateX(${motionSettings.guide_slide_px}px)`},{transform:'translateX(0)'}],motionSettings.guide_slide_ms,false,'cubic-bezier(.333333,0,.666667,1)');
}
function stopTransitions(){for(const running of transitions.values())running.cancel();transitions.clear();}
function setProgress(key,percent){
 if(progressKey!==key){progressKey=key;progressTarget=-1;stopTransition($('systemFill'));$('systemFill').style.width='0%';}
 if(progressTarget===percent)return;
 const width=$('systemProgress').getBoundingClientRect().width,current=width?100*$('systemFill').getBoundingClientRect().width/width:0;
 progressTarget=percent;$('systemFill').style.width=percent+'%';
 animateElement($('systemFill'),[{width:current+'%'},{width:percent+'%'}],motionSettings.progress_ms);
}
let urlTimer;
const initial=new URLSearchParams(location.search);
if(initial.get('scene')==='WIFI_FAILED')initial.set('scene','JOIN');
if(['RECONNECT','PORTAL'].includes(initial.get('scene')))initial.set('scene','READY');
if([...$('scenario').options].some(option=>option.value===initial.get('scene')))$('scenario').value=initial.get('scene');
if(initial.get('orientation')==='portrait')$('orientation').value='portrait';
if(initial.get('clock')==='0')$('clockSynced').checked=false;
if(initial.get('lang')==='en')$('language').value='1';
if(['lan','hotspot','bluetooth'].includes(initial.get('via')))$('reconnectVia').value=initial.get('via');
function resize(){
 const portrait=!guides.includes($('scenario').value)&&$('orientation').value==='portrait',width=portrait?800:1280,height=portrait?1280:800;
 const workspace=document.querySelector('.workspace'),style=getComputedStyle(workspace);
 const available=workspace.clientWidth-parseFloat(style.paddingLeft)-parseFloat(style.paddingRight);
 const availableHeight=Math.max(260,innerHeight-($('screenWrap').getBoundingClientRect().top+scrollY)-24);
 const scale=Math.max(.1,Math.min(1,available/width,availableHeight/height));
 $('screen').style.transform=`scale(${scale})`;$('screenWrap').style.width=width*scale+'px';$('screenWrap').style.height=height*scale+'px';
}
function renderGuide(key,lang,stepChanged){
 const standby=key==='READY'||key==='RECONNECT',via=$('reconnectVia').value;
 const item=standby?{...guide.states[via==='bluetooth'?'BLUETOOTH':'LAN'],step:guide.states[key].step,title:guide.states[key].title}:guide.states[key],english=!!lang;
 $('brand').textContent=english?'Teleprompter-P4':'提词器P4';$('state').textContent=english?'Waiting':'待连接';
 $('title').textContent=item.title[lang];
 $('steps').replaceChildren(...guide.steps.map((labels,i)=>{const el=document.createElement('div');el.className='step'+(i<item.step?' active':'');el.style.width=(100/guide.steps.length)+'%';const number=document.createElement('span');number.className='number';number.textContent=i+1;const label=document.createElement('span');label.textContent=labels[lang];el.append(number,label);return el;}));
 const network=key==='LAN'||standby,lan=network&&via==='lan',bluetooth=key==='BLUETOOTH'||(standby&&via==='bluetooth');
 const useDomain=initial.get('domain')==='1';
 const url=bluetooth?'https://0047ol.github.io/openprompt-p4/':useDomain?'http://teleprompter-demo.local/':lan?'http://192.0.2.10/':'http://192.168.4.1/';
 const deviceName=english?'Teleprompter-P4-4455':'提词器P4-4455';
 const displayUrl=bluetooth?url:lan?'http://192.0.2.10/':'http://192.168.4.1/';
 const values={hotspot:deviceName,ssid:lan?'Home WiFi':deviceName,bluetooth:deviceName,url:displayUrl};
 const paragraphs=item.body[lang].replace(/\{(hotspot|ssid|bluetooth|url)\}/g,(_,field)=>values[field]).split('\n');
 $('description').replaceChildren(...paragraphs.flatMap((text,index)=>{const row=document.createElement('span');row.textContent=text;return index?[document.createTextNode('\n'),row]:[row];}));
 $('description').hidden=!item.body[lang];
 $('guideContent').classList.toggle('centered',!!item.centered);
 $('guideLoader').hidden=!item.loading;
 $('qrFrame').hidden=!item.qr;
 $('qr').dataset.payload=item.qr==='hotspot'?`WIFI:T:nopass;S:${values.hotspot};P:;;`:item.qr?url:'';
 if(item.qr){
  const image=item.qr==='hotspot'?(english?'join-en':'join'):!bluetooth&&useDomain?'local':network?(bluetooth?'bluetooth':lan?'lan':'hotspot'):images[key];
  $('qr').src='device-qr/'+image+'.svg';
 }else $('qr').removeAttribute('src');
 if(stepChanged)slideGuide();
}
function renderSystem(key,lang){
 const english=!!lang,startup=key==='STARTUP',update=key.startsWith('UPDATE_'),percent=Number($('progress').value);
 $('systemScene').classList.toggle('startup',startup);
 const startupCopy=percent<40?['屏幕已启动','Preparing your reading space']:percent<55?['正在启动无线和设备热点','Starting wireless and device hotspot']:percent<65?['正在准备稿件存储','Preparing manuscript storage']:percent<75?['正在读取稿件和样式','Reading manuscripts and styles']:percent<90?['正在启动蓝牙','Wi-Fi ready / Starting Bluetooth']:percent<95?['无线连接已就绪','Wi-Fi + Bluetooth ready']:['正在准备提词界面','Preparing the reading screen'];
 const labels={SHUTDOWN:['正在关机','Shutting down'],RESTART:['正在重新启动','Restarting'],FACTORY_RESET:['正在恢复出厂设置','Restoring factory settings']};
 const title=startup?startupCopy[lang]:update?(english?'Updating':'正在更新'):(labels[key]||['',''])[lang];
 if($('systemTitle').textContent!==title){$('systemTitle').textContent=title;reveal($('systemStatus'));}
 $('systemDetail').hidden=!update;$('systemDetail').textContent=(key==='UPDATE_CHECKING'?['正在检查 SD 卡','Checking SD card']:key==='UPDATE_RESTART'?['校验完成，即将重启','Verified / Restarting']:['正在写入固件','Writing firmware'])[lang];
 $('systemHint').hidden=startup||update||!english;$('systemHint').textContent=key==='SHUTDOWN'?'Press P4 RST to turn on':'Please wait a moment';
 $('systemProgress').hidden=$('systemPercent').hidden=!(startup||update);setProgress(key,percent);$('systemPercent').textContent=percent+'%';
}
async function prepareReading(key,lang,version){
 const portrait=$('orientation').value==='portrait',english=!!lang;
 const font=Math.max(40,Math.min(180,Number($('font').value)||90));
 lastState={revision:version,previewRevision:version,speed:Number($('speed').value),font,rotation:portrait?0:90,brightness:80,letterSpacing:8,lineSpacing:20,countdown:5,mirror:$('mirror').checked?2:0,sightGuide:$('sight').checked,centerHighlight:$('highlight').checked,textAlign:0,marginLeft:8,marginRight:8,textColor:0xffffff,bold:false,showInfo:-1,scroll:0,playing:false,shuttle:1,loopCount:0,playbackElapsedMs:0,countdownRemainingMs:key==='COUNTDOWN'?5000:0,ip:'192.0.2.10'};
 demoScript=english?'Good morning, everyone.\nWelcome to our presentation.\n\nTake a breath, find your pace,\nand let your words flow.\n\nThis is the screen preview.\nYou can adjust the font size,\nswitch orientation and mirror,\nor pause to check the layout.':'各位来宾，大家好。\n欢迎来到今天的分享。\n\n放慢呼吸，找到自己的节奏。\n让每一句话，清晰而从容。\n\n这是提词器屏幕的演示。\n你可以调整字号、切换方向，\n也可以暂停，检查文字排版。';
 lastState.loop=$('demoPlaybackMode').value==='loop';lastState.autoNext=$('demoPlaybackMode').value==='next';lastState.loopCount=6;
 lastState.playing=key==='PLAYING';
 lastState.stopped=key==='STOPPED'||key==='FINISHED';lastState.playbackFinished=key==='FINISHED';
 screenTelemetry={dateTime:demoDateTime(),deviceName:english?'Teleprompter-P4':'提词器P4',mac:'02:11:22:33:44:55',previousTitle:english?'Opening':'开场白',nextTitle:english?'Closing remarks':'结束语',headerState:tr(key==='COUNTDOWN'?'准备中':key==='PLAYING'?'播放中':'已暂停'),fps:key==='PLAYING'?59.8:0,freeMemory:18*1048576,totalMemory:32*1048576,drawnFrames:3180,lateFrames:2,jitterMs:.3,renderMs:2.4,bufferReady:16,bufferTotal:16,pixelBitrate:key==='PLAYING'?560000000:0};
 previewReceivedAt=performance.now();previewDrawSignature='';layoutCache=null;
 await document.fonts.load(`${font}px DeviceReader`);if(version!==generation)return;
 previewLayout=await getDemoLayout();if(version!==generation)return;
 previewLayout.boot=lastState.boot=1;
 const data=new TextEncoder().encode(previewLayout.text);
 previewLayout.rows=previewLayout.lines.map(([start,length,width])=>({text:new TextDecoder().decode(data.slice(start,start+length)).replace(/[\r\n]+$/,''),width}));
 previewLayout.advances=new Map(previewLayout.advances);previewLayout.offsets=new Map();
 fontReady=true;$('fontLoading').hidden=true;renderBarCapacity();paintReading(performance.now());
}
function paintReading(now){
 if(!fontReady||!previewLayout)return;
 const key=$('scenario').value;lastState.scroll=scroll;lastState.playbackElapsedMs=key==='PLAYING'?Math.max(0,now-sceneAt):0;previewReceivedAt=now;previewDrawSignature='';
 if(key==='COUNTDOWN')lastState.countdownRemainingMs=animation?Math.max(1,5000-(now-sceneAt)):3000;
 renderReadingFrame(now);
 if(key==='SLEEP_PENDING'){previewContext.fillStyle=`rgba(0,0,0,${animation?Math.min(1,(now-sceneAt)/motionSettings.sleep_ms):.78})`;previewContext.fillRect(0,0,previewCanvas.width,previewCanvas.height);}
}
function demoDateTime(){
 if(!$('clockSynced').checked)return '';
 const now=new Date();return `${now.getFullYear()}-${String(now.getMonth()+1).padStart(2,'0')}-${String(now.getDate()).padStart(2,'0')}\n${now.toLocaleTimeString('en-GB',{hour12:false,hour:'2-digit',minute:'2-digit'})}`;
}
function render(){
 stopTransitions();exiting=false;document.documentElement.classList.toggle('motion-on',motionEnabled());
 const key=$('scenario').value,lang=Number($('language').value),version=++generation;
 $('screenClock').textContent=demoDateTime().replace('\n',' ');$('screenClock').style.whiteSpace='nowrap';
 uiLanguage=lang?'en':'zh-CN';document.documentElement.lang=uiLanguage;
 $('orientation').disabled=guides.includes(key);
 $('screen').classList.toggle('portrait',!guides.includes(key)&&$('orientation').value==='portrait');
 const header=document.querySelector('.screen-header'),h=deviceSettings.headerHeight,size=Math.min(deviceSettings.headerFontSize,Math.floor((h-8)/1.448));
 header.style.height=h+'px';header.style.fontSize=size+'px';
 $('brand').style.color='#8aa4b8';$('brand').style.fontSize=size+'px';$('brand').hidden=!(deviceSettings.barFields&131072);
 $('guideScene').hidden=!guides.includes(key);$('readingScene').hidden=!reading.includes(key)||key==='ASLEEP';$('systemScene').hidden=guides.includes(key)||reading.includes(key)||key==='OFF';
 $('readingOptions').hidden=!reading.includes(key)||key==='ASLEEP';$('progressOptions').hidden=key!=='STARTUP'&&!key.startsWith('UPDATE_');$('reconnectOptions').hidden=!['LAN','READY','RECONNECT'].includes(key);
 const bluetoothOption=$('reconnectVia').querySelector('[value="bluetooth"]');
 bluetoothOption.hidden=bluetoothOption.disabled=key==='LAN';
 if(key==='LAN'&&$('reconnectVia').value==='bluetooth')$('reconnectVia').value='lan';
 $('connectionModeLabel').textContent=key==='LAN'?'网络方式':'控制方式';
 const notes={ASLEEP:'已休眠：真机背光关闭、全黑，不显示文字。',OFF:'已关机：显示提示后关闭屏幕，此处展示最终黑屏状态。',SLEEP_PENDING:'即将休眠：保留当前画面并降低亮度；自动演示后进入黑屏。',PLAYING:'复用控制台预览的文字、镜像和视线框绘制代码；不代表硬件实测帧率。',STOPPED:'停止后进度归零，显示首行；手动与自动停止共用停止状态。',FINISHED:'播放结束后立即回到开头，保持停止状态，下次播放从首行开始。',COUNTDOWN:'倒计时位于提词区域中央，随屏幕镜像翻转。'};
 $('sceneNote').textContent=notes[key]||'虚拟状态演示，不连接真实设备。扫码内容为示例。';$('progressValue').textContent=$('progress').value+'%';$('speedValue').textContent=$('speed').value;
 $('animate').textContent=animation?'暂停演示':'自动演示';$('animate').setAttribute('aria-pressed',String(animation));$('advance').disabled=key==='OFF';
 const url=new URL(location.href);url.searchParams.set('clock',$('clockSynced').checked?'1':'0');url.searchParams.set('scene',key);url.searchParams.set('orientation',$('orientation').value);url.searchParams.set('lang',lang?'en':'zh');
 if(['LAN','READY','RECONNECT'].includes(key))url.searchParams.set('via',$('reconnectVia').value);else url.searchParams.delete('via');
 clearTimeout(urlTimer);urlTimer=setTimeout(()=>{if(url.href!==location.href)history.replaceState(null,'',url);},200);
 const guideStage=guides.includes(key)?(['READY','RECONNECT'].includes(key)?'READY':key):null;
 if(guideStage!==null)renderGuide(key,lang,lastGuideStage!==null&&lastGuideStage!==guideStage);
 else if(reading.includes(key)&&key!=='ASLEEP'){fontReady=false;$('fontLoading').hidden=false;void prepareReading(key,lang,version).catch(error=>{$('fontLoading').textContent='演示字体加载失败，请刷新重试。';console.error(error);});}
 else if(key!=='ASLEEP'&&key!=='OFF'){renderSystem(key,lang);reveal($('systemMotion'));}
 lastGuideStage=guideStage;
 resize();
}
function next(){const index=order.indexOf($('scenario').value);$('scenario').value=order[(index+1+order.length)%order.length];sceneAt=performance.now();scroll=0;lastSystemProgress=-1;render();}
for(const [label,bit] of barOptions){
 const el=document.createElement('label');el.innerHTML=`<input type="checkbox" data-demo-bar="${bit}">${label}`;
 const input=el.querySelector('input');input.checked=!!(deviceSettings.barFields&bit);
 input.onchange=()=>{selectExclusiveBar(input,'[data-demo-bar]');deviceSettings.barFields=[...document.querySelectorAll('[data-demo-bar]:checked')].reduce((mask,item)=>mask|Number(item.dataset.demoBar),0);render();};$('barDemoFields').append(el);
}
for(const [id,key] of [['demoShowHeader','showHeader'],['demoShowFooter','showFooter'],['demoHeaderHeight','headerHeight'],['demoFooterHeight','footerHeight'],['demoHeaderFont','headerFontSize'],['demoFooterFont','footerFontSize']]){
 const input=$(id);if(input.type==='checkbox')input.checked=deviceSettings[key];else input.value=deviceSettings[key];
 input.onchange=()=>{deviceSettings[key]=input.type==='checkbox'?input.checked:Math.max(Number(input.min),Math.min(Number(input.max),Number(input.value)));input.value=deviceSettings[key];render();};
}
$('demoPlaybackMode').onchange=render;$('barDemoSettings').ontoggle=resize;
for(const id of ['scenario','orientation','language','clockSynced','font','mirror','sight','highlight','reconnectVia'])$(id).addEventListener('change',()=>{sceneAt=performance.now();scroll=0;render();});
$('speed').oninput=()=>{$('speedValue').textContent=$('speed').value;lastState.speed=Number($('speed').value);};$('progress').oninput=()=>{$('progressValue').textContent=$('progress').value+'%';renderSystem($('scenario').value,Number($('language').value));};
$('animate').onclick=()=>{animation=!animation;sceneAt=performance.now();lastFrame=sceneAt;render();};$('rewind').onclick=()=>{scroll=0;sceneAt=performance.now();if($('scenario').value==='PLAYING'){lastState.playing=true;lastState.stopped=false;lastState.playbackFinished=false;}paintReading(performance.now());};$('advance').onclick=next;
$('motion').onchange=render;$('replay').onclick=()=>{sceneAt=performance.now();progressKey='';render();};
reducedMotion.addEventListener('change',()=>{if(reducedMotion.matches)$('motion').checked=false;render();});
function tick(now){
 const dt=Math.min(100,Math.max(0,now-lastFrame));lastFrame=now;const key=$('scenario').value;
 if(animation&&key==='PLAYING'&&lastState.playing){
  scroll+=dt*lastState.speed/1000;
  if(!lastState.loop&&scroll>=(previewLayout?.endScroll??Infinity)){scroll=0;lastState.playing=false;lastState.stopped=true;lastState.playbackFinished=true;}
 }
 if(reading.includes(key)&&key!=='ASLEEP')paintReading(now);
 if(animation){
  if(key==='STARTUP'||key==='UPDATE_WRITING'){const percent=key==='STARTUP'?Math.min(100,Math.round(30+(now-sceneAt)/90)):Math.min(100,Math.round((now-sceneAt)/100));if(percent!==lastSystemProgress){lastSystemProgress=percent;$('progress').value=percent;$('progressValue').textContent=percent+'%';renderSystem(key,Number($('language').value));}if(percent===100&&now-sceneAt>(key==='STARTUP'?7000:10500))next();}
  else if(key==='COUNTDOWN'&&now-sceneAt>=5000)next();else if(key==='SLEEP_PENDING'&&now-sceneAt>=600)next();
  else if(['SHUTDOWN','RESTART','FACTORY_RESET','UPDATE_CHECKING','UPDATE_RESTART'].includes(key)){
   if(['SHUTDOWN','RESTART'].includes(key)&&now-sceneAt>=1200-motionSettings.exit_ms&&!exiting){reveal($('systemMotion'),true);exiting=true;}
   if(now-sceneAt>=(['SHUTDOWN','RESTART','FACTORY_RESET'].includes(key)?1500:1800)){if(key==='RESTART'||key==='FACTORY_RESET'||key==='UPDATE_RESTART'){$('scenario').value='STARTUP';sceneAt=now;lastSystemProgress=-1;render();}else next();}
  }
  else if(guides.includes(key)&&now-sceneAt>=3500)next();
 }requestAnimationFrame(tick);
}
window.addEventListener('resize',()=>requestAnimationFrame(resize));
let observedWidth=0;
new ResizeObserver(entries=>{const width=entries[0].contentRect.width;if(width!==observedWidth){observedWidth=width;resize();}}).observe(document.querySelector('.workspace'));
render();document.fonts.ready.then(resize);requestAnimationFrame(tick);
