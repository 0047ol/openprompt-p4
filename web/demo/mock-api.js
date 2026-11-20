/* Demo-only virtual device. Never calls real Bluetooth, storage or device APIs. */
(()=>{
  'use strict';
  const fixture=window.TP_DEMO_FIXTURE,encode=new TextEncoder(),decode=new TextDecoder();
  if(new URLSearchParams(location.search).get('transport')==='ble'){
    document.documentElement.dataset.defaultTransport='ble';document.documentElement.dataset.hosted='true';
  }
  const clone=value=>structuredClone(value),bytes=value=>encode.encode(value).length;
  const volatileStore=new Map();
  Object.defineProperty(window,'localStorage',{configurable:true,value:{
    getItem:key=>volatileStore.get(String(key))??null,setItem:(key,value)=>volatileStore.set(String(key),String(value)),
    removeItem:key=>volatileStore.delete(String(key)),clear:()=>volatileStore.clear(),
    key:index=>[...volatileStore.keys()][index]??null,get length(){return volatileStore.size;}
  }});
  const defaults={speed:105,font:90,rotation:90,brightness:80,letterSpacing:8,lineSpacing:20,countdown:5,
    loop:true,autoNext:false,bold:false,textAlign:0,marginLeft:8,marginRight:8,textColor:0xffffff,
    mirror:0,sightGuide:true,centerHighlight:true,showInfo:-1};
  const styleKeys=Object.keys(defaults),layoutKeys=['font','rotation','letterSpacing','lineSpacing','marginLeft','marginRight','mirror','textAlign','showInfo'];
  const booleanKeys=['playing','loop','autoNext','bold','sightGuide','centerHighlight'];
  const settings=clone(fixture.settings),wifi=clone(fixture.wifi);wifi.connectionMode='lan';wifi.controllerLimit=0;wifi.networkControllerLimit=0;wifi.bluetoothControllerLimit=0;wifi.bluetoothAlwaysAvailable=true;Object.assign(settings,{autoShutdownSeconds:0,connectionsCoexist:true,controllerLimit:0,networkControllerLimit:0,bluetoothControllerLimit:0,bluetoothAlwaysAvailable:true});
  const accessMode=new URLSearchParams(location.search).get('network')==='hotspot'?'hotspot':'lan';
  let tag=0,revision=1,orderRevision=1,active='',countdownEnd=0,lastTick=performance.now(),layoutCache=null;
  let powered=true,reachable=!new URLSearchParams(location.search).has('offline'),script='',sleepTimer;
  const newTag=()=>`demo-${++tag}`;
  let documents=fixture.documents.map((doc,index)=>({...clone(doc),id:crypto.randomUUID(),order:index,
    etag:newTag(),style:doc.hasStyle?{...defaults}:null}));
  const sdDocuments=documents,internalDocuments=[];
  let internalStorage=new URLSearchParams(location.search).get('storage')==='internal',storageGeneration=1;
  if(internalStorage)documents=internalDocuments;
  settings.timecodeSupported=true;
  const state={...defaults,boot:settings.boot,revision:1,scriptRevision:1,length:0,playing:false,scroll:0,playbackElapsedMs:0,
    previewRevision:1,countdownRemainingMs:0,shuttle:0,loopCount:0,playbackFinished:false,stopped:false,lifecycle:'ready',bleWriteBytes:244};
  const firmware={ok:true,version:'20261118.4',phase:'idle',detail:'firmware_no_new_version',automatic:false,onlineAutomatic:true,factoryResetSupported:true,healthy:true,received:0,size:0};
  const demoUpdate=new URLSearchParams(location.search).get('update');
  const onlineRelease={version:demoUpdate?'20261120.99':firmware.version,forceUpdate:demoUpdate==='required'};
  const onlineImage=new Uint8Array(4096);onlineImage[0]=0xe9;onlineImage[12]=0x12;onlineImage.set(encode.encode(onlineRelease.version),48);
  let sdEjected=new URLSearchParams(location.search).get('storage')==='internal';
  const switchStorage=value=>{if(internalStorage!==value){internalStorage=value;documents=value?internalDocuments:sdDocuments;active='';script='';state.length=0;state.playing=false;++state.scriptRevision;++revision;++storageGeneration;}};
  const portal={mode:'',accepted:false,deadline:0,connectAt:0,clockOffset:0,failWifi:false,networkController:false,closed:false};
  if(location.pathname==='/portal.html')Object.assign(wifi,{connected:false,ip:'',apEnabled:true,lanVerified:false,status:'unconfigured'});
  const portalNow=()=>performance.now()+portal.clockOffset;
  function portalSnapshot(){
    const now=portalNow();
    if(portal.connectAt&&now>=portal.connectAt){
      portal.connectAt=0;
      Object.assign(wifi,portal.failWifi?{connected:false,ip:'',status:'failed'}:{connected:true,ip:'192.0.2.10',status:'connected'});
    }
    const updating=firmware.phase==='uploading'||state.lifecycle==='updating';
    if(portal.deadline&&now>=portal.deadline&&!updating){
      wifi.apEnabled=true;
      portal.deadline=0;
    }
    const remaining=Math.max(0,Math.ceil(portal.deadline-now));
    const phase=updating?'updating':'coexisting';
    return {ok:true,portalOpen:!portal.closed,controllerLimit:0,networkControllerLimit:0,bluetoothControllerLimit:0,bluetoothAlwaysAvailable:true,apName:settings.apName,bleName:settings.bleName,accepted:portal.accepted,
      hotspotUrl:location.origin+'/console.html',controlUrl:location.origin+'/console.html',
      bluetoothUrl:'https://0047ol.github.io/openprompt-p4/',ip:wifi.connected?wifi.ip:'192.168.4.1',mac:'02:00:00:00:00:01',
      connected:wifi.connected,ssid:wifi.ssid,lanVerified:wifi.lanVerified,apEnabled:wifi.apEnabled,status:wifi.status,
      updating,apCloseState:phase,apCloseRemainingMs:remaining};
  }
  function setNetworkMode(mode){
    if(mode==='bluetooth')return;
    portal.deadline=0;wifi.connectionMode=mode;
    if(mode==='hotspot')Object.assign(wifi,{connected:false,ip:'',apEnabled:true,lanVerified:false,status:'unconfigured'});
    else {
      Object.assign(wifi,{configured:true,connected:true,ip:'192.0.2.10',apEnabled:true,lanVerified:false,status:'connected'});
      portal.deadline=0;
    }
  }
  const current=()=>documents.find(doc=>doc.title===active);
  const success=()=>({ok:true,boot:state.boot,revision,mounted:true,active,directory:'/teleprompter',storageError:'ESP_OK',storageBackend:internalStorage?'internal':'sd',storageGeneration,sdMounted:!sdEjected,internalTotalBytes:3900000,internalAvailableBytes:2600000-internalDocuments.reduce((sum,doc)=>sum+bytes(doc.text)+2048,0),
    libraryLayout:settings.libraryLayout,librarySort:settings.librarySort,settingsRevision:settings.revision,
    orderEtag:`demo-order-${orderRevision}`,next:-1});
  const error=reason=>({ok:false,error:reason});
  const style=()=>Object.fromEntries(styleKeys.map(key=>[key,state[key]]));
  function touchLayout(){++state.previewRevision;layoutCache=null;}
  function select(doc){
    active=doc?.title||'';script=doc?.text||'';
    Object.assign(state,defaults,doc?.style||{}, {playing:false,scroll:0,shuttle:0,loopCount:0,countdownRemainingMs:0,playbackElapsedMs:0,playbackFinished:false,stopped:false,length:bytes(script)});
    if(state.loop)state.autoNext=false;
    countdownEnd=0;++revision;++state.revision;++state.scriptRevision;touchLayout();
  }
  select(documents[0]);
  const metadata=doc=>({title:doc.title,text:doc.text,etag:doc.etag,hasStyle:!!doc.style,styleValid:true,
    style:clone(doc.style||defaults),bytes:bytes(doc.text),characters:[...doc.text].length,createdAt:doc.createdAt,updatedAt:doc.updatedAt});
  const item=doc=>({...metadata(doc),text:undefined,style:undefined,id:doc.id,order:doc.order,excerpt:doc.text.slice(0,96)});
  function orderedDocuments(){
    const sort=settings.librarySort;
    return [...documents].sort((a,b)=>{
      if(sort===6)return a.order-b.order;
      if(sort<2)return a.title.localeCompare(b.title,'zh-CN',{numeric:true})*(sort===1?-1:1);
      const key=sort<4?'updatedAt':'createdAt';
      if(!a[key]!==!b[key])return a[key]?-1:1;
      return ((a[key]||0)-(b[key]||0))*(sort%2?1:-1);
    });
  }
  function stopPlayback(finished=false){
    Object.assign(state,{scroll:0,playing:false,shuttle:0,stopped:true,playbackFinished:finished,countdownRemainingMs:0,playbackElapsedMs:0});
    countdownEnd=0;state.rewindRevision=(state.rewindRevision||0)+1;++state.revision;
  }
  function tick(){
    const now=performance.now(),previous=lastTick;lastTick=now;
    state.countdownRemainingMs=Math.max(0,Math.ceil(countdownEnd-now));
    if(!state.playing||settings.asleep)return;
    const elapsed=Math.max(0,now-Math.max(previous,countdownEnd));
    state.playbackElapsedMs+=elapsed;
    state.scroll+=elapsed/1000*state.speed*(state.shuttle||1);
    if(state.scroll<0){stopPlayback();return;}
    const end=layoutCache?(state.loop?layoutCache.loopPeriod:layoutCache.endScroll):Infinity;
    if(state.scroll>=end){
      if(state.loop){const turns=Math.floor(state.scroll/end);state.scroll%=end;state.loopCount+=turns;}
      else if(state.autoNext&&documents.length){
        const ordered=orderedDocuments(),index=ordered.indexOf(current());
        if(index>=0&&index+1<ordered.length){select(ordered[index+1]);state.playing=true;countdownEnd=now+state.countdown*1000;}
        else stopPlayback(true);
      }else stopPlayback(true);
      ++state.revision;
    }
  }
  function control(values){
    if(values.loop===true||Number(values.loop)===1)values={...values,autoNext:0};
    else if(values.autoNext===true||Number(values.autoNext)===1)values={...values,loop:0};
    tick();let changed=false,layoutChanged=false;
    for(const [key,raw] of Object.entries(values)){
      const value=booleanKeys.includes(key)?raw===true||raw===1||raw==='1':Number(raw);
      if(key==='rewind'){stopPlayback();state.loopCount=0;continue;}
      if(![...styleKeys,'playing','shuttle'].includes(key)||!Number.isFinite(Number(value)))continue;
      if(key==='playing'){
        state.shuttle=0;
        if(value)state.stopped=false;
        if(value&&!state.playing){
          if(state.playbackFinished){state.scroll=0;state.playbackFinished=false;}
          countdownEnd=performance.now()+(state.scroll===0?state.countdown*1000:0);
        }else if(!value)countdownEnd=0;
      }
      if(key==='shuttle'&&state.countdownRemainingMs>0)continue;
      if(key==='shuttle'){state.playing=true;state.playbackFinished=false;state.stopped=false;}
      if(state[key]!==value){changed=changed||styleKeys.includes(key);layoutChanged=layoutChanged||layoutKeys.includes(key);state[key]=value;}
      if(key==='brightness')defaults[key]=state[key];
    }
    state.countdownRemainingMs=Math.max(0,Math.ceil(countdownEnd-performance.now()));
    if(changed&&current()){current().style=style();current().etag=newTag();++revision;}
    if(layoutChanged)touchLayout();++state.revision;return clone(state);
  }
  function updateSettings(updates){
    for(const key of ['autoSleepSeconds','autoShutdownSeconds','language','showInfo','apName','bleName','libraryLayout','librarySort','showHeader','showFooter','barFields','headerHeight','footerHeight','headerFontSize','footerFontSize'])
      if(updates[key]!==undefined)settings[key]=updates[key];
    if(updates.showInfo!==undefined)settings.showHeader=settings.showFooter=updates.showInfo;
    settings.showInfo=settings.showHeader||settings.showFooter;
    if(['showHeader','showFooter','headerHeight','footerHeight','showInfo'].some(key=>updates[key]!==undefined))touchLayout();
    if(updates.apPassword!==undefined){settings.apPasswordPending=true;settings.apOpen=!updates.apPassword;}
    settings.barFields&=~(64|8192|65536);
    for(const key of ['apName','bleName'])if(['Teleprompter-P4','提词器','提词器P4','Teleprompter-P4-4455','提词器-4455','提词器P4-4455'].includes(settings[key])){
      settings[key]=(settings.language==='en'?'Teleprompter-P4':'提词器P4')+'-4455';
      settings[key==='apName'?'activeApName':'activeBleName']=settings[key];
    }
    settings.restartRequired=settings.apName!==settings.activeApName||settings.bleName!==settings.activeBleName||settings.apPasswordPending;
    ++settings.revision;
  }
  function screen(asleep){
    if(settings.asleep===asleep&&state.lifecycle===(asleep?'asleep':'ready'))return;
    settings.asleep=asleep;state.lifecycle=asleep?'asleep':'ready';++settings.revision;++state.revision;
    if(asleep){state.playing=false;countdownEnd=0;state.countdownRemainingMs=0;}
  }
  function deviceAction(action){
    clearTimeout(sleepTimer);
    if(action==='standby'||action==='wake'){
      state.lifecycle=action==='standby'?'sleeping':'waking';++state.revision;
      sleepTimer=setTimeout(()=>screen(action==='standby'),600);
    }else if(action==='restart'||action==='shutdown'){
      state.lifecycle=action==='restart'?'restarting':'shutting_down';state.playing=false;countdownEnd=0;++state.revision;
      setTimeout(()=>{powered=false;device.gatt.disconnect();},500);
      if(action==='restart')setTimeout(()=>{
        powered=true;++state.boot;settings.boot=state.boot;state.lifecycle='ready';
        settings.activeApName=settings.apName;settings.activeBleName=settings.bleName;
        settings.apPasswordPending=false;settings.restartRequired=false;screen(false);
        firmware.phase='idle';firmware.detail='firmware_no_new_version';
      },1800);
    }else throw Error('invalid_request');
  }
  async function preview(){
    if(layoutCache)return clone(layoutCache);
    await document.fonts.load(`${state.font}px DeviceReader`);
    const portrait=state.rotation%180===0,width=(portrait?800:1280)-state.marginLeft-state.marginRight;
    const override=state.showInfo>=0;
    const screenHeight=portrait?1280:800;
    const viewportTop=((override?state.showInfo:settings.showHeader)?settings.headerHeight:0)+8;
    const bottom=((override?state.showInfo:settings.showFooter)?settings.footerHeight:0)+8;
    const height=screenHeight-viewportTop-bottom,center=screenHeight/2-viewportTop,lineHeight=Math.round(state.font*1.448),baseline=lineHeight-Math.round(state.font*.288);
    const ctx=document.createElement('canvas').getContext('2d');ctx.font=`${state.font}px DeviceReader`;ctx.fontKerning='none';
    const advances=new Map(),lines=[];let start=0,offset=0,rowWidth=0,rowLength=0;
    for(const char of script){
      const length=bytes(char),cp=char.codePointAt(0),advance=Math.round(ctx.measureText(char).width);
      advances.set(cp,advance);
      if(char==='\n'){lines.push([start,offset-start+length,rowWidth]);offset+=length;start=offset;rowWidth=0;rowLength=0;continue;}
      const step=advance+(rowLength?state.letterSpacing:0);
      if(rowLength&&rowWidth+step>width){lines.push([start,offset-start,rowWidth]);start=offset;rowWidth=0;rowLength=0;}
      rowWidth+=advance+(rowLength?state.letterSpacing:0);++rowLength;offset+=length;
    }
    if(offset>start||!lines.length)lines.push([start,offset-start,rowWidth]);
    const encoded=new TextEncoder().encode(script),textDecoder=new TextDecoder();let playbackRows=lines.length;
    while(playbackRows&&!textDecoder.decode(encoded.slice(lines[playbackRows-1][0],lines[playbackRows-1][0]+lines[playbackRows-1][1])).trim())--playbackRows;
    const lineStep=lineHeight+state.lineSpacing,loopPeriod=Math.max(1,playbackRows)*lineStep;
    const bandHeight=Math.min(height-8,lineHeight+32);
    const readingCenter=state.mirror&2?height-center:center;
    const textTop=Math.floor(readingCenter+Math.floor(bandHeight/2)+state.lineSpacing);
    const endScroll=playbackRows?Math.max(0,playbackRows-1)*lineStep+textTop-(readingCenter-Math.floor(lineHeight/2)):0;
    layoutCache={ok:true,revision:state.previewRevision,scriptRevision:state.scriptRevision,rotation:state.rotation,text:script,advances:[...advances],lines,
      font:state.font,lineHeight,lineSpacing:state.lineSpacing,letterSpacing:state.letterSpacing,
      marginLeft:state.marginLeft,marginRight:state.marginRight,mirror:state.mirror,textAlign:state.textAlign,
      width,height,baseline,playbackRows,loopPeriod,endScroll,viewportTop,textTop,bandTop:Math.floor(center-bandHeight/2),bandHeight};
    return clone(layoutCache);
  }
  async function library(q){
    // Visual review needs a real paint opportunity on both virtual transports.
    // Delay whole requests, never every BLE packet or background status read.
    if(['list','retry','get','select','create','save','delete','reorder','preview'].includes(q.op))await wait(320);
    if(q.op==='preview')return preview();
    if(q.op==='telemetry'){const ordered=orderedDocuments(),index=ordered.indexOf(current());return {ok:true,dateTime:new Date().toLocaleString('sv-SE').replace(' ','\n'),deviceName:settings.apName,mac:'02:11:22:33:44:55',previousTitle:ordered[index-1]?.title||'',nextTitle:ordered[index+1]?.title||'',fps:state.playing?59.8:0,jitterMs:state.playing?.3:0,renderMs:state.playing?2.4:0,bufferReady:12,bufferTotal:12,scroll:state.scroll,freeMemory:18*1048576,totalMemory:32*1048576,drawnFrames:state.playing?120:0,lateFrames:0,pixelBitrate:state.playing?600000000:0};}
    if(q.op==='wifi'){
      if(q.action==='mode'){setNetworkMode(q.mode);return {ok:true,...wifi};}
      if(q.action==='save'){
        if(q.network){
          const previous=wifi.network||{},next=q.network;
          if(next.staticIp===true&&!previous.staticIp){
            if(!wifi.connected||!wifi.lease?.gateway||!wifi.lease?.netmask||!wifi.lease?.dns)return error('invalid_network_settings');
            wifi.network={...previous,...wifi.lease};
          }
          wifi.network={...wifi.network,...next,supported:true,pending:false};
          if('hostname' in next||'mdnsEnabled' in next){wifi.network.mdnsAvailable=!!wifi.network.mdnsEnabled&&!!wifi.network.hostname;wifi.localDomain=wifi.network.mdnsAvailable?wifi.network.hostname+'.local':'';}
          if('apIp' in next)wifi.apIp=next.apIp;
        }
        if(q.ssid!==undefined)Object.assign(wifi,{connectionMode:'lan',configured:true,connected:true,ssid:q.ssid,apEnabled:true,lanVerified:false,status:'connected'});
        if(wifi.connected)wifi.ip=wifi.network?.staticIp?wifi.network.ip:'192.0.2.10';
        portal.deadline=0;
      }else if(q.action==='forget'){
        Object.assign(wifi,{connectionMode:'hotspot',configured:false,connected:false,ssid:'',ip:'',apEnabled:true,lanVerified:false,status:'unconfigured'});
        portal.deadline=0;
      }else if(q.action!=='status')return error('invalid_request');
      return {ok:true,...portalSnapshot(),...wifi};
    }
    if(q.op==='defaults')return {...success(),style:clone(defaults)};
    if(q.op==='presence'){
      settings.playbackClients=q.reading?1:0;clearTimeout(sleepTimer);
      settings.standbyGuide=!q.reading;screen(false);if(!q.reading){state.playing=false;state.countdownRemainingMs=0;countdownEnd=0;}
      return success();
    }
    if(q.op==='firmware')return firmwareRequest(q);
    if(q.op==='storage')return {...success(),sdTotalBytes:32*1024**3,sdFreeBytes:30*1024**3};
    if(q.op==='sd'){
      if(q.action==='format'&&!q.confirmed)return error('confirmation_required');
      if(q.action==='eject'){sdEjected=true;switchStorage(true);}
      if(q.action==='format'){sdDocuments.splice(0);active='';script='';state.playing=false;state.length=0;++state.scriptRevision;sdEjected=false;}
      if(q.action==='mount'){sdEjected=false;switchStorage(false);}
      if(q.action==='internal')switchStorage(true);
      return {...success(),mounted:!sdEjected,ejected:sdEjected,state:sdEjected?'ejected':'ready',...(!sdEjected?{sdTotalBytes:32*1024**3,sdFreeBytes:30*1024**3,sdUsedBytes:2*1024**3}:{})};
    }
    if(q.op==='deviceSettings')return {ok:true,...clone(settings)};
    if(q.op==='list'||q.op==='retry'){
      if(sdEjected)switchStorage(true);
      const terms=String(q.query||'').toLowerCase().split(/\s+/).filter(Boolean);
      const items=documents.filter(doc=>(!q.titles||q.titles.includes(doc.title))&&terms.every(term=>(doc.title+' '+doc.text).toLowerCase().includes(term)));
      const offset=Math.max(0,Number(q.offset)||0);
      const limit=q.index?96:48;
      return {...success(),compact:!!q.index,total:items.length,items:items.slice(offset,offset+limit).map(doc=>{
        const value=item(doc);if(q.index){delete value.excerpt;value.characters=null;}return value;
      }),next:offset+limit<items.length?offset+limit:-1};
    }
    if(q.op==='reorder'){
      if(q.expected!==`demo-order-${orderRevision}`)return error('order_conflict');
      if(!Array.isArray(q.ids)||q.ids.length!==documents.length||new Set(q.ids).size!==documents.length||q.ids.some(id=>!documents.some(doc=>doc.id===id)))return error('invalid_order');
      q.ids.forEach((id,index)=>{documents.find(doc=>doc.id===id).order=index;});++revision;++orderRevision;
      settings.librarySort=6;++settings.revision;return success();
    }
    if((q.storageBackend&&q.storageBackend!==(internalStorage?'internal':'sd'))||(q.storageGeneration&&q.storageGeneration!==storageGeneration))return error('storage_changed');
    const doc=documents.find(entry=>entry.title===q.title);
    if(q.op==='create'||q.op==='save'){
      if(typeof q.title!=='string'||!q.title.trim()||q.title.trim()!==q.title||bytes(q.title)>96||/[\x00-\x1f/\\:*?"<>|]/.test(q.title)||q.title.startsWith('.')||q.title.endsWith('.'))return error('invalid_title');
      if(typeof q.text!=='string'||bytes(q.text)>8192)return error('invalid_text');
      if(q.op==='create'&&doc)return error('already_exists');
    }
    if(q.op==='create'){
      const next={title:q.title,text:q.text,createdAt:q.timestamp,updatedAt:null,id:crypto.randomUUID(),
        order:Math.max(-1,...documents.map(entry=>entry.order))+1,etag:newTag(),style:q.style?clone(q.style):null};
      documents.push(next);++revision;++orderRevision;return {...success(),etag:next.etag};
    }
    if(!doc&&['get','select','save','delete'].includes(q.op))return error('missing_or_invalid_script');
    if(q.op==='get')return {...success(),...metadata(doc)};
    if(q.op==='select'){select(doc);return success();}
    if(q.op==='save'||q.op==='delete'){
      if(q.expected!==doc.etag)return error('conflict');
      if(q.op==='delete'){
        documents.splice(documents.indexOf(doc),1);if(active===doc.title)select(null);++revision;++orderRevision;return success();
      }
      doc.text=q.text;doc.updatedAt=q.timestamp;doc.etag=newTag();
      if(q.style!==undefined)doc.style=q.style?clone(q.style):null;
      if(active===doc.title){script=doc.text;state.length=bytes(script);state.scroll=0;++state.scriptRevision;touchLayout();if(q.style!==undefined)Object.assign(state,doc.style||defaults);++state.revision;}
      ++revision;return {...success(),etag:doc.etag};
    }
    return error('invalid_request');
  }
  function firmwareRequest(q){
    if(q.action==='factory_reset'){
      if(q.confirmed!==true)return error('confirmation_required');
      if(['uploading','downloading','checking','restarting','resetting'].includes(firmware.phase))return error('firmware_busy');
      Object.assign(firmware,{phase:'resetting',detail:''});state.lifecycle='resetting';state.playing=false;countdownEnd=0;++state.revision;
      setTimeout(()=>{powered=false;device.gatt.disconnect();},500);
      setTimeout(()=>{
        Object.assign(settings,clone(fixture.settings),{boot:state.boot+1,revision:settings.revision+1,autoSleepSeconds:0,autoShutdownSeconds:0,libraryLayout:0,librarySort:6,apOpen:true,activeApOpen:true,apPasswordPending:false,restartRequired:false,asleep:false});
        Object.assign(wifi,{configured:false,connected:false,ssid:'',ip:'',apIp:'192.168.4.1',apEnabled:true,status:'unconfigured',lanVerified:false,connectionMode:'hotspot',localDomain:'',network:{supported:true,apIp:'192.168.4.1',staticIp:false,ip:'',netmask:'255.255.255.0',gateway:'',dns:'',hostname:'teleprompter-4455',mdnsEnabled:false,mdnsAvailable:false,pending:false}});
        Object.assign(state,defaults,{boot:settings.boot,length:0,playing:false,stopped:false,scroll:0,lifecycle:'ready',countdownRemainingMs:0});
        script='';active='';touchLayout();Object.assign(firmware,{phase:'idle',automatic:false,onlineAutomatic:true,received:0,size:0});powered=true;
      },1800);
    }else if(firmware.phase==='resetting'&&q.action!=='status')return error('factory_reset_busy');
    else if(q.action==='settings'){if(q.automatic!==undefined)firmware.automatic=!!q.automatic;if(q.onlineAutomatic!==undefined)firmware.onlineAutomatic=!!q.onlineAutomatic;}
    else if(q.action==='scan')Object.assign(firmware,{phase:'idle',detail:'firmware_no_new_version'});
    else if(q.action==='begin'){
      if(!Number.isInteger(q.size)||q.size<288||q.size>0x5f0000)return error('firmware_invalid_size');
      Object.assign(firmware,{phase:'uploading',detail:'',size:q.size,received:0,token:1});state.lifecycle='updating';
      firmware.targetVersion=decode.decode(Uint8Array.from(atob(q.data),c=>c.charCodeAt(0)).slice(48,80)).split('\0')[0];
    }else if(q.action==='chunk'){
      if(q.token!==firmware.token||firmware.phase!=='uploading')return error('firmware_invalid_session');
      if(q.offset!==firmware.received)return error('firmware_invalid_offset');
      const size=atob(q.data).length;if(firmware.received+size>firmware.size)return error('firmware_invalid_data');firmware.received+=size;
    }else if(q.action==='finish'){
      if(q.token!==firmware.token||firmware.phase!=='uploading')return error('firmware_invalid_session');
      if(firmware.received!==firmware.size)return error('firmware_incomplete');firmware.version=firmware.targetVersion||firmware.version;firmware.phase='restarting';deviceAction('restart');
    }else if(q.action==='cancel'){firmware.phase='idle';firmware.detail='firmware_cancelled';state.lifecycle='ready';}
    else if(!['status','settings','scan'].includes(q.action))return error('invalid_request');
    return clone(firmware);
  }
  const originalFetch=window.fetch.bind(window);
  const response=(value,status=200)=>new Response(JSON.stringify(value),{status,headers:{'Content-Type':'application/json'}});
  function ensureOnline(){if(!reachable||!powered)throw new TypeError('Failed to fetch');}
  const wait=(ms,signal)=>new Promise((resolve,reject)=>{
    const abort=()=>{clearTimeout(timer);reject(new DOMException('Demo request aborted','AbortError'));};
    const timer=setTimeout(()=>{signal?.removeEventListener('abort',abort);resolve();},ms);
    if(signal?.aborted)abort();else signal?.addEventListener('abort',abort,{once:true});
  });
  window.fetch=async(input,options={})=>{
    const url=new URL(typeof input==='string'||input instanceof URL?input:input.url,location.href);
    if(url.pathname.endsWith('/firmware/latest.json'))return response({project:'teleprompter_p4',chip:'esp32p4',version:onlineRelease.version,file:'teleprompter-p4-'+onlineRelease.version+'.bin',size:onlineImage.length,sha256:sha256Bytes(onlineImage),forceUpdate:onlineRelease.forceUpdate});
    if(url.pathname.endsWith('/firmware/teleprompter-p4-'+onlineRelease.version+'.bin')){
      const range=/^bytes=(\d+)-(\d+)$/.exec(new Headers(options.headers).get('Range')||'');
      return new Response(range?onlineImage.slice(Number(range[1]),Number(range[2])+1):onlineImage,{status:range?206:200});
    }
    if(url.origin!==location.origin)throw new TypeError('Demo blocks external requests');
    if(!url.pathname.startsWith('/api/'))return originalFetch(input,options);
    const method=(options.method||input.method||'GET').toUpperCase();
    await wait(url.pathname==='/api/library'?0:90,options.signal);ensureOnline();tick();
    const json=()=>JSON.parse(options.body||'{}');
    switch(url.pathname){
      case '/api/status':return response(state);
      case '/api/settings':if(method==='POST')updateSettings(json());return response(settings);
      case '/api/wifi':{
        if(method==='POST'){
          const body=new URLSearchParams(options.body||'');
          const q={op:'wifi',action:body.get('action')||'save'};
          for(const key of ['ssid','password','mode'])if(body.has(key))q[key]=body.get(key);
          if(body.has('network'))q.network=JSON.parse(body.get('network'));
          return response({...await library(q),accessMode});
        }
        return response({...portalSnapshot(),...wifi,accessMode});
      }
      case '/api/library':return response(await library(method==='GET'?{op:'list'}:json()));
      case '/api/control':return response(control(Object.fromEntries(url.searchParams)));
      case '/api/script':
        if(method==='PUT'){script=String(options.body||'');state.length=bytes(script);++state.scriptRevision;++state.revision;touchLayout();return response(state);}
        return new Response(script,{headers:{'Content-Type':'text/plain;charset=utf-8'}});
      case '/api/device':deviceAction(url.searchParams.get('action'));return response({ok:true});
      case '/api/display/retry':return response({ok:true,...state});
      case '/api/portal':return response({ok:false,error:'portal_removed',controlUrl:'/'},410);
      default:return response(error('invalid_request'),404);
    }
  };
  // Preserve the controller's BLE mode and run its actual GATT chunk protocol.
  const serviceId='7d2a0001-4b7c-4d40-9e22-2f0b8d0c3a11';
  const asView=value=>{const data=encode.encode(JSON.stringify(value));return new DataView(data.buffer);};
  class Characteristic{
    constructor(id){this.id=id;this.properties={writeWithoutResponse:true};this.buffer=new Uint8Array();this.reply=new Uint8Array();this.offset=0;}
    async readValue(){
      this.check();tick();if(this.id===2)return asView(state);
      if(this.id===5){
        // Match the firmware's ATT-limited discovery record, not the full RPC.
        const compact=clone(settings);
        for(const key of ['connectionsCoexist','startupPortal','controllerLimit','connectionMode','networkControllerLimit','bluetoothControllerLimit','bluetoothAlwaysAvailable','portalClientLimit','apNameDefault','bleNameDefault','apOpen','activeApOpen','defaultNamePending','webClients','playbackClients','showHeader','showFooter','headerHeight','footerHeight','barFields','headerFontSize','footerFontSize'])delete compact[key];
        return asView({...compact,settingsChunked:true});
      }
      if(this.id===6){
        const chunk=this.reply.slice(this.offset,this.offset+236),view=new DataView(new ArrayBuffer(8+chunk.length));
        view.setUint32(0,this.reply.length,true);view.setUint32(4,this.offset,true);new Uint8Array(view.buffer,8).set(chunk);return view;
      }
      throw new DOMException('Characteristic cannot be read','NotSupportedError');
    }
    check(){ensureOnline();if(!device.gatt.connected)throw new DOMException('Demo Bluetooth disconnected','NetworkError');}
    async writeValueWithResponse(value){return this.write(value);}
    async writeValueWithoutResponse(value){return this.write(value);}
    async write(value){
      this.check();const data=ArrayBuffer.isView(value)?new Uint8Array(value.buffer,value.byteOffset,value.byteLength):new Uint8Array(value);
      const view=new DataView(data.buffer,data.byteOffset,data.byteLength),op=data[0];
      if(this.id===3){
        if(op===10){deviceAction({1:'restart',2:'standby',3:'wake',5:'shutdown'}[data[1]]);return;}
        const key={1:'playing',2:'speed',3:'font',4:'rotation',5:'rewind',6:'brightness',7:'letterSpacing',8:'lineSpacing',9:'countdown',11:'loop',12:'textAlign',13:'marginLeft',14:'marginRight',15:'textColor',16:'mirror',17:'sightGuide',18:'centerHighlight',19:'shuttle',20:'autoNext',21:'bold',22:'showInfo'}[op];
        if(!key)throw Error('invalid_request');
        const number=op===15?data[1]|data[2]<<8|data[3]<<16:op===19?view.getInt8(1):[2,4,13,14].includes(op)?view.getUint16(1,true):data[1]??1;
        control({[key]:number});return;
      }
      const base=this.id===6?0x20:0;
      if(op===base){this.buffer=new Uint8Array(view.getUint16(1,true));return;}
      if(op===base+1){this.buffer.set(data.subarray(3),view.getUint16(1,true));return;}
      if(op===base+2){
        if(this.id===6)this.reply=encode.encode(JSON.stringify(await library(JSON.parse(decode.decode(this.buffer)))));
        else if(this.id===5)updateSettings(JSON.parse(decode.decode(this.buffer)));
        else {script=decode.decode(this.buffer);state.length=bytes(script);++state.revision;touchLayout();}return;
      }
      if(this.id===6&&op===0x23){this.offset=view.getUint32(1,true);return;}
      throw Error('invalid_request');
    }
  }
  const handles=new Map([2,3,4,5,6].map(id=>[id,new Characteristic(id)]));
  const device=new EventTarget();device.id='teleprompter-demo';device.name=settings.bleName;
  device.gatt={connected:false,
    async connect(){ensureOnline();this.connected=true;return this;},
    disconnect(){if(!this.connected)return;this.connected=false;const event=new Event('gattserverdisconnected');device.dispatchEvent(event);device.ongattserverdisconnected?.(event);},
    async getPrimaryService(uuid){
      if(uuid!==serviceId)throw new DOMException('Unknown service','NotFoundError');
      return {getCharacteristic:async uuid=>{
        const id=Number(uuid[7]);if(!handles.has(id)||uuid!==`7d2a000${id}-4b7c-4d40-9e22-2f0b8d0c3a11`)throw new DOMException('Unknown characteristic','NotFoundError');return handles.get(id);
      }};
    }
  };
  const bluetooth=new EventTarget();bluetooth.getAvailability=async()=>true;
  bluetooth.getDevices=async()=>reachable?[device]:[];
  bluetooth.requestDevice=async()=>{reachable=true;powered=true;return device;};
  Object.defineProperty(navigator,'bluetooth',{configurable:true,value:bluetooth});
  class DemoSocket extends EventTarget{
    static CONNECTING=0;static OPEN=1;static CLOSING=2;static CLOSED=3;
    constructor(url){
      super();const address=new URL(url);if(address.host!==location.host||address.pathname!=='/api/live')throw Error('Demo blocks external sockets');
      this.readyState=0;this.openTimer=setTimeout(()=>{
        this.readyState=1;this.onopen?.(new Event('open'));
        this.interval=setInterval(()=>{if(reachable&&powered){tick();this.emit(state);}},250);
      },30);
    }
    emit(value){const event=new MessageEvent('message',{data:JSON.stringify(value)});this.onmessage?.(event);this.dispatchEvent(event);}
    send(value){
      if(this.readyState!==1)throw new DOMException('Socket not open','InvalidStateError');
      if(!reachable||!powered){this.close();return;}const q=JSON.parse(value);
      setTimeout(()=>{
        if(this.readyState!==1)return;
        if(!reachable||!powered){this.close();return;}
        if(q.heartbeat){tick();this.emit(state);}
        else this.emit({id:q.id,state:control(Object.fromEntries(new URLSearchParams(q.query)))});
      },60);
    }
    close(){clearTimeout(this.openTimer);clearInterval(this.interval);this.readyState=3;const event=new Event('close');this.onclose?.(event);this.dispatchEvent(event);}
  }
  Object.defineProperty(window,'WebSocket',{configurable:true,value:DemoSocket});
  window.TP_DEMO={snapshot:()=>clone({state,settings,wifi,documents,firmware}),
    portal:updates=>{if(updates?.advanceMs)portal.clockOffset+=updates.advanceMs;if(updates?.failWifi!==undefined)portal.failWifi=updates.failWifi;
      if(updates?.closed!==undefined)portal.closed=updates.closed;
      if(updates?.networkController!==undefined){portal.networkController=updates.networkController;if(updates.networkController){portal.closed=true;if(wifi.connected&&wifi.apEnabled)portal.deadline=0;}}
      if(updates?.lanVerified!==undefined)wifi.lanVerified=updates.lanVerified;
      if(updates?.updating!==undefined){firmware.phase=updates.updating?'uploading':'idle';state.lifecycle=updates.updating?'updating':'ready';}
      return clone(portalSnapshot());},
    disconnect:()=>{reachable=false;device.gatt.disconnect();},
    reconnect:()=>{reachable=true;powered=true;state.lifecycle='ready';},
    library:async request=>clone(await library(request))};
  // Sample IP links must also remain inside the local Demo.
  document.addEventListener('click',event=>{
    const anchor=event.target.closest('a[href]');if(!anchor)return;
    if([wifi.apIp,wifi.ip,wifi.localDomain].filter(Boolean).includes(new URL(anchor.href,location.href).hostname)){event.preventDefault();location.href='/console.html';}
  });
})();
