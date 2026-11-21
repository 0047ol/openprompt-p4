let firmwareOnlinePending=false,firmwareOnlineNotice=null;
let factoryResetConfirmPending=false;
var firmwarePollAt=0,firmwareTransfer=false,firmwareCancelled=false,firmwareState=null;
let firmwarePageVersion=null,firmwarePageDevice=null,firmwarePageRefresh=null,firmwarePageRefreshTimer=0;
function scheduleFirmwarePageRefresh(state){
  const device=mode==='ble'?'ble:'+bleDevice?.id:'lan:'+location.host;
  if(firmwarePageDevice!==device){firmwarePageDevice=device;firmwarePageVersion=null;firmwarePageRefresh=null;}
  if(!onlineVersion(state.version))return;
  if(firmwarePageVersion===null){firmwarePageVersion=state.version;return;}
  if(state.version===firmwarePageVersion||state.healthy!==true||state.phase!=='idle'||!online)return;
  firmwarePageRefresh={version:state.version,device};
  clearTimeout(firmwarePageRefreshTimer);
  if(documentDirty){
    $('firmwareRefreshText').textContent=tr('固件已更新。请先保存或放弃未保存的稿件，再刷新页面。');
    statusBanner($('firmwareRefreshNotice'),state.version);
    return;
  }
  statusBanner($('firmwareRefreshNotice'),null);
  if(document.hidden||firmwareTransfer||busy||$('confirmDialog').open)return;
  firmwarePageRefreshTimer=setTimeout(()=>refreshFirmwarePage(),600);
}
function refreshFirmwarePage(){
  const pending=firmwarePageRefresh;
  if(!pending||!online||documentDirty||firmwareTransfer||busy||document.hidden||$('confirmDialog').open||pending.device!==firmwarePageDevice)return;
  const url=new URL(location.href);url.searchParams.set('fw',pending.version);
  // This changes the document cache key; all controller CSS/JS are inlined.
  // Save only a transient connection hint, never manuscript content/config.
  try{if(mode==='ble'&&bleDevice?.id)sessionStorage.setItem('teleprompter-update-device',bleDevice.id);}catch{}
  firmwarePageRefresh=null;location.replace(url.href);
}
$('firmwareRefreshButton').onclick=async()=>{
  if(documentDirty){
    if(!await confirmAction(tr('刷新页面？'),tr('未保存的稿件修改将丢失。建议先保存稿件，再刷新。'),{danger:true,confirmLabel:tr('放弃并刷新')}))return;
    documentDirty=false;
  }
  refreshFirmwarePage();
};
function firmwareAwaitingPortal(){return !online&&['restarting','resetting'].includes(firmwareState?.phase)&&['restart','factory_reset'].includes(lifecycleIntent?.action)&&performance.now()-lifecycleIntent.at>10000;}
function firmwareLocked(){return (sdStatusEpoch===epoch&&sdState?.state==='formatting')||(!firmwareAwaitingPortal()&&(firmwareTransfer||['uploading','checking','restarting','resetting'].includes(firmwareState?.phase)||['updating','resetting'].includes(lastState?.lifecycle)));}
function updateFirmwareLock(){
  const locked=firmwareLocked();document.body.dataset.updating=String(locked);
  for(const selector of ['#homePage','#playPage','#editPage','#settingsPage','.workspace-head','#blePanel']){
    const element=document.querySelector(selector);if(!element)continue;
    element.inert=locked;element.setAttribute('aria-busy',String(locked));
  }
  if(locked&&$('confirmDialog').open)$('confirmDialog').close('cancel');
  if(locked)for(const dialog of document.querySelectorAll('.bar-fields-dialog[open],#diagnosticsDialog[open]'))dialog.close();
}
for(const name of ['click','input','change','submit','keydown'])document.addEventListener(name,event=>{
  if(!firmwareLocked())return;
  if(event.target.closest('button,input,textarea,select,a,form,[role=button]')){event.preventDefault();event.stopImmediatePropagation();}
},true);
function renderDeviceActivity(){
  renderDevicePower();
  renderCrashRecovery();
  const notice=$('activityNotice'),state=firmwareState;
  const updating=state?.phase==='uploading'||state?.phase==='downloading'||state?.phase==='checking'||firmwareTransfer||lastState?.lifecycle==='updating',restarting=state?.phase==='restarting';
  const lifecycle=lastState?.lifecycle;
  const power=devicePowerPhase(),awaiting=firmwareAwaitingPortal()||(!online&&lifecycleIntent?.action==='restart'&&performance.now()-lifecycleIntent.at>10000);
  let title='',description='',kind='';
  if(awaiting){
    // The persistent offline connection banner provides the recovery guidance.
  }else if(state?.phase==='resetting'||lifecycle==='resetting'||lifecycleIntent?.action==='factory_reset'){
    kind='factory_reset';title=tr('正在恢复出厂设置');
  }else if(updating||restarting){
    kind='firmware';
    title=tr(restarting?'更新已校验，正在重启':'正在更新固件');
    description=tr('请勿断电或断开连接。');
  }else if(lifecycleIntent?.action==='restart'||lifecycle==='restarting'){
    kind='restart';title=tr('设备正在重启');
  }else if(['powering_off','powering_on'].includes(power)||power==='shutting_down'&&online){
    kind=power;title=tr(power==='powering_on'?'设备正在开机':'正在关机…');
  }else if(online&&lifecycle==='starting'){
    kind='startup';title=tr('设备正在启动');
  }
  notice.dataset.activity=kind;
  const copy=title+(description?' · '+description:'');
  if($('activityTitle').textContent!==copy)$('activityTitle').textContent=copy;
  const starting=power==='powering_on';
  const percent=starting?Math.max(0,Math.min(100,Math.floor(lastState?.powerProgress??0))):state?.size?Math.min(100,Math.max(0,Math.floor((state.received||0)/state.size*100))):0;
  const progress=$('activityProgress');
  const showProgress=!!title&&(starting||updating);
  progress.hidden=!showProgress;progress.value=percent;
  progress.setAttribute('aria-label',tr(starting?'开机进度':'固件更新进度'));
  $('activityPercent').hidden=!showProgress;
  $('activityPercent').textContent=percent+'%';
  statusToast(notice,title?epoch+':'+kind:null);
  updateFirmwareLock();
  return !!title;
}
const firmwareMessages={
  firmware_already_installed:'此固件已安装，无需重复更新',firmware_not_newer:'请选择比当前版本更新的固件',
  firmware_wrong_chip:'固件不是 ESP32-P4 固件',firmware_wrong_project:'固件不属于此提词器项目',
  firmware_invalid_header:'固件信息无效',firmware_header_incomplete:'固件文件不完整',
  firmware_verification_failed:'固件校验失败，已保留原版本',firmware_invalid_size:'固件大小不符合要求',
  firmware_busy:'设备正在处理另一项更新',firmware_starting:'设备尚在启动，请稍后重试',
  firmware_failed_before:'此固件曾启动失败，已阻止重复更新',firmware_rolled_back:'新固件启动失败，已回退原版本',
  firmware_available:'发现可用更新',firmware_no_new_version:'没有更新版本的固件',
  firmware_read_failed:'读取固件失败',firmware_write_failed:'写入固件失败，原版本保留',
  firmware_upload_timeout:'上传超时，已取消',firmware_cancelled:'已取消更新',no_sd_card:'未检测到可用 SD 卡',
  firmware_header_changed:'固件信息在传输中发生变化，请重新选择文件',firmware_incomplete:'固件传输尚未完成',
  firmware_save_failed:'无法保存更新记录，已保留原版本',firmware_select_failed:'无法切换固件，已保留原版本',
  firmware_begin_failed:'无法开始固件更新，请稍后重试',firmware_directory_unavailable:'无法读取 SD 卡固件目录',
  firmware_invalid_session:'更新会话已结束，请重新开始',firmware_invalid_offset:'固件传输位置不匹配，请重新开始',
  firmware_invalid_data:'固件传输数据无效',firmware_file_exists:'SD 卡已有同名固件，未覆盖现有文件',
  firmware_staged:'固件已校验并保存到 SD 卡，尚未安装',
  factory_reset_busy:'设备正在恢复出厂设置，请稍候',factory_reset_failed:'恢复出厂设置未能启动，请重试'
};
function firmwareText(code){return tr(firmwareMessages[code]||code);}
function renderFirmwareStatus(){
  const state=firmwareState;if(!state)return;
  const working=['uploading','downloading','checking','restarting','resetting'].includes(state.phase);
  const notice=firmwareOnlineNotice;
  if(!working&&state.phase!=='error'&&!state.available&&notice?.epoch===epoch&&notice.version===state.version){
    $('firmwareStatus').textContent=typeof notice.text==='function'?notice.text():tr(notice.text);
    $('firmwareStatus').dataset.state=notice.kind;return;
  }
  $('firmwareStatus').textContent=state.phase==='resetting'?tr('正在恢复出厂设置'):state.phase==='restarting'?tr('更新已校验，正在重启…'):state.phase==='checking'?tr('正在检查 SD 卡…'):state.phase==='downloading'?tr('正在下载固件…'):working?tr('正在写入固件…'):firmwareText(state.detail)||tr('就绪');
  $('firmwareStatus').dataset.state=working?'working':state.detail&&!['firmware_available','firmware_no_new_version','firmware_already_installed','firmware_cancelled'].includes(state.detail)?'notice':'ready';
  if(state.available)$('firmwareStatus').textContent+=' · '+state.available;
}
function renderFirmware(state){
  const previous=firmwareState;
  firmwareState=state;
  const progress=state.phase==='uploading'||state.phase==='downloading';
  $('firmwareVersion').textContent='v'+state.version;
  $('firmwareVersion').setAttribute('aria-label',tr('当前版本：')+state.version);
  renderFirmwareStatus();
  if(document.activeElement!==$('firmwareAutomatic'))$('firmwareAutomatic').checked=state.automatic;
  if(document.activeElement!==$('firmwareOnlineAutomatic'))$('firmwareOnlineAutomatic').checked=state.onlineAutomatic!==false;
  scheduleOnlineFirmwareCheck();
  $('firmwareProgress').hidden=!progress;
  $('firmwareProgress').value=state.size?Math.min(1,Math.max(0,(state.received||0)/state.size)):0;
  firmwareButtons();
  buttons();
  if(state.phase==='error'&&(previous?.phase!=='error'||previous?.detail!==state.detail))message(firmwareText(state.detail),true);
  scheduleFirmwarePageRefresh(state);
}
function firmwareButtons(){
  sdButtons();
  const phase=firmwareState?.phase;
  const unavailable=!online||devicePowerBlocked()||firmwareTransfer||['uploading','checking','restarting','resetting'].includes(phase);
  for(const id of ['firmwareLocal','firmwareScan','firmwareAutomatic','firmwareOnlineAutomatic','firmwareOnline'])setControlDisabled($(id),unavailable,busy||(id==='firmwareOnline'&&firmwareOnlinePending));
  setControlDisabled($('deviceFactoryReset'),unavailable||firmwareState?.factoryResetSupported!==true,busy||factoryResetConfirmPending);
  $('deviceFactoryReset').dataset.tooltip=firmwareState?.factoryResetSupported===true?'恢复机器配置，保留 SD 卡稿件和独立样式。':'请先更新设备固件以使用恢复出厂设置。';
}
async function firmwareRequest(values){
  try{return await libraryRequest({op:'firmware',...values});}
  catch(error){if(error.name==='AbortError')throw error;throw Error(userError(firmwareText(error.message)));}
}
async function pollFirmware(){
  if(sdState?.state==='formatting'){await refreshSdStatus();return;}
  const active=['uploading','checking','restarting','resetting'].includes(firmwareState?.phase)||['updating','resetting'].includes(lastState?.lifecycle);
  const version=epoch,interval=active?1000:pageName==='settings'?15000:60000;
  if(firmwareTransfer||(firmwareState&&performance.now()-firmwarePollAt<interval))return;
  firmwarePollAt=performance.now();
  const request=queue.then(()=>version===epoch&&online?firmwareRequest({action:'status'}):null);queue=request.catch(()=>{});
  try{const state=await request;if(state&&version===epoch)renderFirmware(state);}catch(error){if(version!==epoch)return;const reason=userError(error);if(reason)$('firmwareStatus').textContent=reason;}
  if(pageName==='settings'&&version===epoch)await refreshSdStatus();
}
var sdState=null,sdRequestPending=false,sdStatusAt=0,sdStatusEpoch=-1;
function renderSdStatus(state){
  sdState=state;sdStatusEpoch=epoch;
  $('sdStatus').textContent=state.detail?tr(libraryErrors[state.detail]||state.detail):tr(state.state==='formatting'?'正在格式化 SD 卡…':state.mounted?'SD 卡已插入，状态正常':state.ejected?'SD 卡已安全弹出，可取出存储卡':state.sdFilesystem==='exfat'?(state.sdExfatSupported?'exFAT 读取失败，原卡数据保留，可先使用内置存储':'SD 卡为 exFAT，当前支持 FAT32；原卡数据保留，可先使用内置存储'):'SD 卡不可用，请检查插卡、文件系统或读卡连接');
  const internal=state.storageBackend==='internal';
  $('internalStorageStatus').hidden=!state.storageBackend;
  $('internalStorageStatus').textContent=tr(internal?'当前稿件库：内置存储':state.storageBackend==='unavailable'?'当前稿件库：不可用':'当前稿件库：SD 卡')+(Number.isFinite(state.internalAvailableBytes)?' · '+tr('内置可用：')+documentSize(state.internalAvailableBytes):'');
  $('internalStorage').hidden=internal||!state.storageBackend;
  const internalTotal=state.internalTotalBytes,internalUsed=state.internalUsedBytes;
  const internalCapacity=Number.isFinite(internalTotal)&&internalTotal>0&&Number.isFinite(internalUsed)&&Number.isFinite(state.internalAvailableBytes);
  $('internalCapacity').hidden=$('internalUsage').hidden=!internalCapacity;
  if(internalCapacity){
    const used=Math.max(0,Math.min(internalTotal,internalUsed)),percent=used/internalTotal*100;
    $('internalTotal').textContent=tr('总容量：')+documentSize(internalTotal);
    $('internalUsed').textContent=tr('已用：')+documentSize(used);
    $('internalFree').textContent=tr('可用：')+documentSize(Math.max(0,state.internalAvailableBytes));
    $('internalUsageBar').value=percent;
    $('internalUsagePercent').textContent=documentSize(used)+' / '+documentSize(internalTotal);
    $('internalUsageBar').setAttribute('aria-valuetext',$('internalUsed').textContent+' / '+$('internalTotal').textContent);
  }
  const total=state.sdTotalBytes;
  const rawUsed=Number.isFinite(state.sdUsedBytes)?state.sdUsedBytes:total-state.sdFreeBytes;
  const capacity=state.mounted!==false&&Number.isFinite(total)&&total>0&&Number.isFinite(rawUsed);
  $('sdCapacity').hidden=!capacity;
  $('sdUsage').hidden=!capacity;
  if(capacity){
    const used=Math.min(total,Math.max(0,rawUsed)),free=total-used,percent=used/total*100;
    $('sdTotal').textContent=tr('总容量：')+documentSize(total);
    $('sdUsed').textContent=tr('已用：')+documentSize(used);
    $('sdFree').textContent=tr('可用：')+documentSize(free);
    $('sdUsageBar').value=percent;
    $('sdUsagePercent').textContent=documentSize(used)+' / '+documentSize(total);
    $('sdUsageBar').setAttribute('aria-valuetext',$('sdUsed').textContent+' / '+$('sdTotal').textContent);
  }
  sdButtons();updateFirmwareLock();
}
function sdButtons(){
  const unavailable=!online||firmwareLocked()||devicePowerBlocked(),pending=busy||sdRequestPending;
  setControlDisabled($('sdMount'),unavailable,pending);
  setControlDisabled($('internalStorage'),unavailable,pending);
  for(const id of ['sdEject','sdFormat'])setControlDisabled($(id),unavailable||!sdState?.mounted,pending);
}
async function refreshSdStatus(force=false){
  if(sdStatusEpoch!==epoch){sdState=null;sdStatusAt=0;}
  const formatting=sdState?.state==='formatting';
  if(!online||sdRequestPending||(!formatting&&firmwareLocked())||(!force&&sdStatusEpoch===epoch&&performance.now()-sdStatusAt<(formatting?2000:15000)))return;
  const generation=epoch;sdRequestPending=true;sdButtons();
  try{const state=await libraryRequest({op:'sd',action:'status'});if(generation===epoch){sdStatusEpoch=epoch;sdStatusAt=performance.now();renderSdStatus(state);}}
  catch(error){if(generation===epoch)$('sdStatus').textContent=userError(error);}
  finally{sdRequestPending=false;sdButtons();}
}
async function sdAction(action){
  if(!online||sdRequestPending||firmwareLocked())return;
  const generation=epoch;
  if(action==='format'){
    if(!await confirmAction(tr('格式化 SD 卡？'),tr('将删除整张 SD 卡上的所有文件，包括稿件、样式和固件，无法恢复。'),{danger:true,delay:5}))return;
    if(!await confirmAction(tr('确认清空整张 SD 卡？'),tr('请确认重要文件已备份。设备系统设置不会被删除。'),{danger:true}))return;
  }else if(action==='eject'&&!await confirmAction(tr('安全弹出 SD 卡？'),tr('将停止播放并关闭存储卡。随后将切换到内置稿件库，SD 卡原稿件保留。')))return;
  if(['internal','mount'].includes(action)&&libraryState?.active&&!await confirmAction(tr('切换稿件存储？'),tr('将停止当前提词。两处稿件独立保存，不会自动复制或覆盖。')))return;
  if(generation!==epoch||!online)return;
  sdRequestPending=true;sdButtons();
  const finish=beginRegionLoading($('sdSettings'),tr(action==='format'?'正在格式化 SD 卡…':action==='eject'?'正在安全弹出…':'正在读取 SD 卡状态…'));
  try{await run(async()=>{
    let state=await libraryRequest({op:'sd',action,confirmed:action==='format'});renderSdStatus(state);
    const deadline=performance.now()+120000;
    while(state.state==='formatting'){
      if(generation!==epoch||!online)return;
      if(performance.now()>deadline)throw Error(tr('SD 卡仍在处理中，请稍后重新读取状态'));
      await new Promise(resolve=>setTimeout(resolve,700));
      state=await libraryRequest({op:'sd',action:'status'});renderSdStatus(state);
    }
    libraryPollAt=0;await refreshLibrary();
  });}
  catch(error){
    message(error,true);
    if(generation===epoch&&online){
      // A failed SD reprobe can still switch the usable library to internal storage.
      // Refresh that state without replaying the card operation or any write.
      sdStatusAt=0;libraryPollAt=0;
      await Promise.allSettled([refreshLibrary(),libraryRequest({op:'sd',action:'status'}).then(state=>{if(generation===epoch)renderSdStatus(state);})]);
    }
  }
  finally{finish();sdRequestPending=false;sdButtons();}
}
$('internalStorage').onclick=()=>sdAction('internal');
$('sdMount').onclick=()=>sdAction('mount');$('sdEject').onclick=()=>sdAction('eject');$('sdFormat').onclick=()=>sdAction('format');
$('deviceFactoryReset').onclick=async()=>{
  if(!online||busy||factoryResetConfirmPending||firmwareLocked()||firmwareState?.factoryResetSupported!==true)return;
  const generation=epoch;factoryResetConfirmPending=true;firmwareButtons();
  try{
    if(!await confirmAction(tr('恢复出厂设置？'),tr('将恢复设备、网络、播放默认值和更新偏好，并重启设备。SD 卡中的稿件、独立样式和排序文件保留。重启后需重新连接设备热点。'),{danger:true,delay:5}))return;
    if(generation!==epoch||!online)return;
    await run(async()=>{
      const state=await firmwareRequest({action:'factory_reset',confirmed:true});
      if(generation!==epoch)return;
      lifecycleIntent={action:'factory_reset',boot:lastState?.boot,at:performance.now()};
      renderFirmware(state);renderDeviceActivity();
    });
  }catch(error){message(error,true);}
  finally{factoryResetConfirmPending=false;firmwareButtons();}
};
$('firmwareAutomatic').onchange=()=>run(async()=>{
  const enabled=$('firmwareAutomatic').checked;
  renderFirmware(await firmwareRequest({action:'settings',automatic:enabled,...(enabled?{onlineAutomatic:false}:{})}));message(tr('设置已保存'));
}).catch(()=>{});
$('firmwareOnlineAutomatic').onchange=()=>run(async()=>{
  const enabled=$('firmwareOnlineAutomatic').checked;
  renderFirmware(await firmwareRequest({action:'settings',onlineAutomatic:enabled,...(enabled?{automatic:false}:{})}));message(tr('设置已保存'));
}).catch(()=>{});
$('firmwareScan').onclick=()=>run(async()=>{
  const version=epoch,deadline=performance.now()+30000;
  let state=await firmwareRequest({action:'scan'});renderFirmware(state);
  while(state.phase==='checking'){
    if(version!==epoch||!online)return;
    if(performance.now()>=deadline)throw new DOMException('Firmware scan timeout','TimeoutError');
    await new Promise(resolve=>setTimeout(resolve,250));
    if(version!==epoch||!online)return;
    state=await firmwareRequest({action:'status'});renderFirmware(state);
  }
  if(state.phase==='error')throw Error(firmwareText(state.detail));
  if(!state.available){message(firmwareText(state.detail)||tr('没有更新版本的固件'));return;}
  if(!await confirmAction(tr('安装固件并重启？'),state.available+'\n'+tr('设备重启后会尝试自动重连并刷新页面。'),{danger:true}))return;
  renderFirmware(await firmwareRequest({action:'sd'}));
}).catch(()=>{});
$('firmwareLocal').onclick=()=>$('firmwareFile').click();
$('firmwareCancel').onclick=()=>{firmwareCancelled=true;};
function bytesBase64(bytes){let binary='';for(const byte of bytes)binary+=String.fromCharCode(byte);return btoa(binary);}
async function installFirmwareFile(file,confirmed=false){
  if(!file)return;
  if(file.size<288||file.size>0x5f0000){message(tr('固件大小不符合要求'),true);return;}
  const head=new Uint8Array(await file.slice(0,288).arrayBuffer());
  if(head[0]!==0xe9||head[12]!==0x12||head[13]!==0){message(tr('固件不是 ESP32-P4 固件'),true);return;}
  const version=decoder.decode(head.slice(48,80)).split('\0')[0];
  if(!confirmed&&!await confirmAction(tr('安装固件并重启？'),`${file.name}\n${tr('版本：')}${version}\n${mode==='ble'?tr('蓝牙传输较慢，请保持页面打开。'):tr('更新完成后设备会自动重启。')}\n${tr('设备重启后会尝试自动重连并刷新页面。')}`,{danger:true}))return;
  firmwareTransfer=true;firmwareCancelled=false;let token=0,installed=false;
  $('firmwareCancel').hidden=true;updateFirmwareLock();
  await run(async()=>{
    // run() already serializes this complete transfer with other requests.
      try{
        const begin=await firmwareRequest({action:'begin',size:file.size,data:bytesBase64(head)});token=begin.token;renderFirmware(begin);
        const chunkSize=mode==='ble'&&blePayloadSize()===17?768:3072;
        for(let offset=0;offset<file.size;offset+=chunkSize){
          if(firmwareCancelled)throw Error(tr('已取消更新'));
          const chunk=new Uint8Array(await file.slice(offset,offset+chunkSize).arrayBuffer());
          renderFirmware(await firmwareRequest({action:'chunk',token,offset,data:bytesBase64(chunk)}));
        }
        renderFirmware(await firmwareRequest({action:'finish',token}));token=0;installed=true;
        lifecycleIntent={action:'restart',boot:lastState?.boot,at:performance.now()};renderDeviceActivity();
      }finally{if(token)try{renderFirmware(await firmwareRequest({action:'cancel',token}));}catch{firmwarePollAt=0;}}
  }).catch(()=>{});
  firmwareTransfer=false;$('firmwareCancel').hidden=true;firmwarePollAt=0;buttons();
  return installed;
 }
$('firmwareFile').onchange=async()=>{
  const file=$('firmwareFile').files[0];$('firmwareFile').value='';
  await installFirmwareFile(file);
};
const onlineFirmwareBase='https://0047ol.github.io/openprompt-p4/firmware/';
function onlineVersion(value){
  const match=/^(\d{8})\.(\d{1,6})$/.exec(value||'');
  return match?Number(match[1])*1000000+Number(match[2]):0;
}
// Diagnostic builds report a suffix, but their base version is still known.
// Release manifests remain strict: a diagnostic image is never a public OTA.
function currentFirmwareVersion(value){return onlineVersion(String(value||'').replace(/-sdtest$/,''));}
async function externalFirmwareRequest(url,progress,signal,range){
  const abort=new AbortController(),cancel=()=>abort.abort();
  let timedOut=false,stallTimer;
  const timeout=()=>{timedOut=true;abort.abort();};
  const timer=setTimeout(timeout,progress?15*60000:6000);
  // A slow but advancing download must not hit the old two-minute deadline.
  // Bound both a stalled connection and the total transfer independently.
  const received=()=>{if(progress){clearTimeout(stallTimer);stallTimer=setTimeout(timeout,60000);}};
  received();
  if(signal?.aborted)abort.abort();else signal?.addEventListener('abort',cancel,{once:true});
  try{
    const response=await fetch(url,{signal:abort.signal,credentials:'omit',referrerPolicy:'no-referrer',cache:'no-cache',...(range?{headers:{Range:`bytes=${range.start}-${range.end}`}}:{})});
    if(!response.ok)throw Error(tr('固件服务器暂不可用，请稍后重试'));
    if(range&&response.status!==206)throw Error(tr('固件下载不完整，请重试'));
    received();
    // Read the body under the same deadline, not just the response headers.
    const limit=range?range.end-range.start+1:progress?0x5f0000:16384;
    if(Number(response.headers.get('Content-Length'))>limit)throw Error(tr('服务器固件信息无效'));
    if(!response.body){const bytes=new Uint8Array(await response.arrayBuffer());if(bytes.length>limit||(range&&bytes.length!==limit))throw Error(tr('服务器固件信息无效'));return bytes;}
    const reader=response.body.getReader(),chunks=[];let size=0;
    try{
      for(;;){const {done,value}=await reader.read();if(done)break;size+=value.length;
        if(size>limit)throw Error(tr('固件大小不符合要求'));
        if(value.length)received();
        chunks.push(value);if(progress)progress(size);
      }
    }finally{reader.releaseLock();}
    if(range&&size!==limit)throw Error(tr('固件下载不完整，请重试'));
    const bytes=new Uint8Array(size);let offset=0;for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}return bytes;
  }catch(error){
    abort.abort();
    if(timedOut)throw Error(tr(progress?'固件下载超时，请检查网络后重试。':'检查更新超时，请稍后重试。'));
    throw error;
  }finally{clearTimeout(timer);clearTimeout(stallTimer);signal?.removeEventListener('abort',cancel);}
}
async function downloadFirmwareRelease(release,progress,signal){
  // Bound each public response and retry only the missing range. The full image
  // is still checked against the manifest SHA-256 before installation.
  const bytes=new Uint8Array(release.size),chunkSize=256*1024;
  const partial=new Array(Math.ceil(release.size/chunkSize)).fill(0);
  const abort=new AbortController(),cancel=()=>abort.abort();
  const timer=setTimeout(cancel,15*60000);let next=0,reported=0;
  if(signal?.aborted)cancel();else signal?.addEventListener('abort',cancel,{once:true});
  const notify=(index,size)=>{partial[index]=Math.max(partial[index],size);reported=Math.max(reported,partial.reduce((a,b)=>a+b,0));progress?.(reported);};
  try{
    const workers=Array.from({length:Math.min(2,partial.length)},async()=>{
      while(next<partial.length){
        const index=next++,start=index*chunkSize,end=Math.min(release.size,start+chunkSize)-1;
        for(let attempt=0;;attempt++){
          if(abort.signal.aborted)throw new DOMException('Aborted','AbortError');
          try{
            const chunk=await externalFirmwareRequest(onlineFirmwareBase+release.file,size=>notify(index,size),abort.signal,{start,end});
            bytes.set(chunk,start);notify(index,chunk.length);break;
          }catch(error){
            if(abort.signal.aborted||attempt>=2)throw error;
            await new Promise(resolve=>setTimeout(resolve,250*(attempt+1)));
          }
        }
      }
    });
    // Abort sibling requests on a terminal failure, and await their cleanup.
    let failure;
    await Promise.allSettled(workers.map(worker=>worker.catch(error=>{failure??=error;cancel();throw error;})));
    if(failure)throw failure;
    return bytes;
  }finally{clearTimeout(timer);signal?.removeEventListener('abort',cancel);}
}
let firmwareCheckTask=null,firmwareCheckAbort=null,firmwareCheckTimer=0,firmwareCheckEpoch=-1;
let firmwareCheckNext=0,firmwareCheckFailures=0,firmwareOnlineRelease=null;
let firmwareUpdateNotice=null,firmwareUpdateNoticeTimer=0,firmwareUpdateNoticeOpen=false;
const firmwareUpdateNotified=new Set();
function scheduleFirmwareUpdateNotice(){
  clearTimeout(firmwareUpdateNoticeTimer);
  const notice=firmwareUpdateNotice;
  if(!notice||notice.epoch!==epoch||!online||(!notice.release.forceUpdate&&firmwareState?.onlineAutomatic===false)||document.hidden||firmwareLocked())return;
  if(onlineVersion(notice.release.version)<=currentFirmwareVersion(firmwareState?.version)){firmwareUpdateNotice=null;return;}
  firmwareUpdateNoticeTimer=setTimeout(async()=>{
    if(notice!==firmwareUpdateNotice)return;
    if(busy||firmwareOnlinePending||firmwareUpdateNoticeOpen||$('confirmDialog').open){scheduleFirmwareUpdateNotice();return;}
    if(notice.epoch!==epoch||!online||document.hidden||firmwareLocked()||(!notice.release.forceUpdate&&firmwareState?.onlineAutomatic===false))return;
    firmwareUpdateNotice=null;firmwareUpdateNoticeOpen=true;firmwareUpdateNotified.add(notice.key);
    try{
      let retry=false;
      const required=notice.release.forceUpdate===true;
      const current=()=>notice.epoch===epoch&&online&&onlineVersion(notice.release.version)>currentFirmwareVersion(firmwareState?.version);
      do{
        // A lost/replaced device invalidates this modal; it must never update a new target.
        const connectionWatch=setInterval(()=>{if(!current()&&$('confirmDialog').open)$('confirmDialog').close('connection-changed');},250);
        let accepted;
        try{accepted=await confirmAction(tr(required?'必须更新固件':'发现固件更新'),
          tr('当前版本：')+firmwareState.version+' → '+notice.release.version+'\n'+documentSize(notice.release.size)+'\n'+
          tr(required?(retry?'更新未完成，请重试。完成更新后才能继续使用。':'此版本为强制更新，完成更新后才能继续使用。'):'下载校验完成后安装，设备会自动重启。'),
          {danger:true,confirmLabel:tr(retry?'重试更新':'下载并安装'),cancelLabel:tr('稍后'),dismissible:!required});}
        finally{clearInterval(connectionWatch);}
        if(!current())break;
        if(!accepted){if(required){retry=true;continue;}break;}
        firmwareOnlinePending=true;firmwareButtons();
        let installed=false;
        try{installed=await installOnlineFirmware(notice.release,true);}finally{firmwareOnlinePending=false;firmwareButtons();}
        if(installed)break;
        retry=true;
      }while(required&&current());
    }finally{firmwareUpdateNoticeOpen=false;}
  },600);
}
function offerFirmwareUpdate(release){
  const key=String(epoch)+':'+String(lastState?.boot??'device')+':'+release.version;
  if(!release.forceUpdate&&firmwareUpdateNotified.has(key))return;
  if(firmwareUpdateNoticeOpen)return;
  firmwareUpdateNotice={release,key,epoch};scheduleFirmwareUpdateNotice();
}
function onlineFirmwareStatus(text,kind='ready'){firmwareOnlineNotice={text,kind,epoch,version:firmwareState?.version};renderFirmwareStatus();}
function scheduleOnlineFirmwareCheck(){
  clearTimeout(firmwareCheckTimer);
  scheduleFirmwareUpdateNotice();
  if(!online||!firmwareState||firmwareLocked()||document.hidden)return;
  if(firmwareCheckEpoch!==epoch){firmwareCheckEpoch=epoch;firmwareCheckNext=0;firmwareOnlineRelease=null;firmwareCheckAbort?.abort();}
  if(navigator.onLine===false){onlineFirmwareStatus('当前浏览器离线；本地文件和 SD 卡更新仍可使用。','notice');return;}
  firmwareCheckTimer=setTimeout(async()=>{
    if(!online||document.hidden||firmwareLocked())return;
    if(busy||firmwareOnlinePending){scheduleOnlineFirmwareCheck();return;}
    await checkOnlineFirmware(true);scheduleOnlineFirmwareCheck();
  },Math.max(1800,firmwareCheckNext-Date.now()));
}
async function fetchOnlineRelease(){
  if(firmwareCheckTask)return firmwareCheckTask;
  const generation=epoch,abort=new AbortController();firmwareCheckAbort=abort;
  firmwareCheckTask=(async()=>{
    const metadata=JSON.parse(decoder.decode(await externalFirmwareRequest(onlineFirmwareBase+'latest.json',null,abort.signal)));
    if(generation!==epoch||!online)throw new DOMException('Connection changed','AbortError');
    if(metadata.project!=='teleprompter_p4'||metadata.chip!=='esp32p4'||!onlineVersion(metadata.version)||
       !Number.isInteger(metadata.size)||metadata.size<288||metadata.size>0x5f0000||
       !/^[a-f0-9]{64}$/.test(metadata.sha256)||metadata.file!=='teleprompter-p4-'+metadata.version+'.bin'||
       (metadata.forceUpdate!==undefined&&typeof metadata.forceUpdate!=='boolean'))
      {const error=Error(tr('服务器固件信息无效'));error.updateMetadata=true;throw error;}
    metadata.forceUpdate=metadata.forceUpdate===true;
    firmwareCheckFailures=0;firmwareCheckNext=Date.now()+15*60000;firmwareOnlineRelease=metadata;
    return metadata;
  })();
  try{return await firmwareCheckTask;}finally{firmwareCheckTask=null;if(firmwareCheckAbort===abort)firmwareCheckAbort=null;}
}
window.addEventListener('offline',()=>{firmwareCheckAbort?.abort();clearTimeout(firmwareCheckTimer);clearTimeout(firmwareUpdateNoticeTimer);onlineFirmwareStatus('当前浏览器离线；本地文件和 SD 卡更新仍可使用。','notice');});
window.addEventListener('online',()=>{firmwareCheckNext=0;scheduleOnlineFirmwareCheck();});
document.addEventListener('visibilitychange',()=>{if(document.hidden){clearTimeout(firmwareCheckTimer);clearTimeout(firmwareUpdateNoticeTimer);clearTimeout(firmwarePageRefreshTimer);}else{scheduleOnlineFirmwareCheck();if(firmwareState)scheduleFirmwarePageRefresh(firmwareState);}});
window.addEventListener('pagehide',()=>{clearTimeout(firmwareCheckTimer);clearTimeout(firmwareUpdateNoticeTimer);clearTimeout(firmwarePageRefreshTimer);firmwareCheckAbort?.abort();});
$('firmwareOnline').onclick=async()=>{
  if(!online||busy||firmwareLocked()||firmwareOnlinePending)return;
  firmwareOnlinePending=true;firmwareButtons();
  try{await checkOnlineFirmware();}finally{firmwareOnlinePending=false;firmwareButtons();}
};
async function checkOnlineFirmware(automatic=false){
  const version=epoch,current=firmwareState?.version;
  if(!currentFirmwareVersion(current)){onlineFirmwareStatus('请等待当前固件版本读取完成','notice');return;}
  if(navigator.onLine===false){onlineFirmwareStatus('当前浏览器离线；本地文件和 SD 卡更新仍可使用。','notice');if(!automatic)message(tr('当前浏览器离线；本地文件和 SD 卡更新仍可使用。'),true);return;}
  const finishLoading=automatic?()=>{}:beginRegionLoading($('firmwareSettings'),tr('正在检查服务器版本…'));
  let release;
  try{
    const metadata=await fetchOnlineRelease();
    if(version!==epoch||!online)return;
    if(onlineVersion(metadata.version)<=currentFirmwareVersion(current)){onlineFirmwareStatus('当前已是最新版本');if(!automatic)message(tr('当前已是最新版本'));return;}
    onlineFirmwareStatus(()=>tr('发现新版本：')+metadata.version+' · '+tr('点击在线更新后确认安装'),'working');
    if(metadata.forceUpdate||automatic){if(metadata.forceUpdate||firmwareState?.onlineAutomatic!==false)offerFirmwareUpdate(metadata);return;}
    release=metadata;
  }catch(error){
    if(version!==epoch||!online)return;
    if(error.updateMetadata){onlineFirmwareStatus('服务器固件信息无效','notice');return;}
    firmwareCheckNext=Date.now()+Math.min(15*60000,60000*2**Math.min(4,firmwareCheckFailures++));
    // The browser downloads updates, not the P4. A successful device AP
    // connection does not establish internet access on the controlling phone.
    const hotspot=mode==='lan'&&wifiState?.apIp===location.hostname;
    onlineFirmwareStatus(navigator.onLine===false?'当前浏览器无法联网；本地文件和 SD 卡更新仍可使用。':hotspot?'当前连接设备热点，浏览器未能访问更新服务器；请使用可联网的网络后重试。本地文件和 SD 卡更新仍可使用。':'更新服务器暂时无法访问，请稍后重试；本地文件和 SD 卡更新仍可使用。','notice');
    return;
  }finally{finishLoading();}
  if(release)await installOnlineFirmware(release);
}
async function installOnlineFirmware(release,confirmed=false){
  const version=epoch,current=firmwareState?.version;
  if(!online||busy||firmwareLocked()||onlineVersion(release.version)<=onlineVersion(current))return;
  if(!confirmed&&!await confirmAction(tr('下载并安装固件？'),
    tr('当前版本：')+current+' → '+release.version+'\n'+documentSize(release.size)+'\n'+tr('下载校验完成后安装，设备会自动重启。'),{danger:true,confirmLabel:tr('下载并安装')}))return;
  if(version!==epoch||!online)return;
  firmwareTransfer=true;updateFirmwareLock();
  try{
    renderFirmware({...firmwareState,phase:'downloading',size:release.size,received:0});
    const bytes=await downloadFirmwareRelease(release,received=>renderFirmware({...firmwareState,phase:'downloading',size:release.size,received}));
    if(bytes.length!==release.size)throw Error(tr('固件下载不完整，请重试'));
    const digest=await firmwareSha256(bytes);
    if(digest!==release.sha256)throw Error(tr('固件校验失败，已保留原版本'));
    if(decoder.decode(bytes.slice(48,80)).split('\0')[0]!==release.version)throw Error(tr('服务器固件信息无效'));
    if(version!==epoch||!online)throw Error(tr('设备连接已变化，请重新检查更新'));
    return await installFirmwareFile(new File([bytes],release.file,{type:'application/octet-stream'}),true);
  }catch(error){message(error,true);return false;}
  finally{firmwareTransfer=false;if(firmwareState?.phase==='downloading')firmwareState={...firmwareState,phase:'idle'};firmwarePollAt=0;buttons();}
}
// Web Crypto is unavailable on an HTTP LAN page. This small SHA-256 fallback
// performs the same content check before any bytes are written to the device.
async function firmwareSha256(bytes){
  if(globalThis.crypto?.subtle){const value=await crypto.subtle.digest('SHA-256',bytes);return Array.from(new Uint8Array(value),b=>b.toString(16).padStart(2,'0')).join('');}
  return sha256Bytes(bytes);
}
// SHA-256 over downloaded bytes, with bounded padding and uint32 arithmetic.
function sha256Bytes(bytes){
  const constants=new Uint32Array([0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2]);
  const hash=new Uint32Array([0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19]);
  const buffer=new Uint8Array(Math.ceil((bytes.length+9)/64)*64),words=new Uint32Array(64);
  buffer.set(bytes);buffer[bytes.length]=128;const view=new DataView(buffer.buffer);
  view.setUint32(buffer.length-8,Math.floor(bytes.length/0x20000000));view.setUint32(buffer.length-4,(bytes.length*8)>>>0);
  const rotate=(n,b)=>(n>>>b)|(n<<(32-b));
  for(let offset=0;offset<buffer.length;offset+=64){
    for(let i=0;i<16;i++)words[i]=view.getUint32(offset+i*4);
    for(let i=16;i<64;i++){const a=words[i-15],b=words[i-2];words[i]=(words[i-16]+(rotate(a,7)^rotate(a,18)^(a>>>3))+words[i-7]+(rotate(b,17)^rotate(b,19)^(b>>>10)))>>>0;}
    let [a,b,c,d,e,f,g,h]=hash;
    for(let i=0;i<64;i++){
      const one=(h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+constants[i]+words[i])>>>0;
      const two=((rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c)))>>>0;
      h=g;g=f;f=e;e=(d+one)>>>0;d=c;c=b;b=a;a=(one+two)>>>0;
    }
    [a,b,c,d,e,f,g,h].forEach((n,i)=>hash[i]=(hash[i]+n)>>>0);
  }
  return Array.from(hash,n=>n.toString(16).padStart(8,'0')).join('');
}
