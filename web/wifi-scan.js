// Discovery uses the same HTTP/BLE library transport; credentials keep the
// existing asynchronous network handoff and never require a device reboot.
let wifiScanState=null,wifiScanBusy=false,wifiScanReading=false,wifiScanGeneration=0,wifiScanSignature='',wifiChoice=null,wifiJoin=null;
const wifiScanButton=document.createElement('button');wifiScanButton.id='wifiScan';wifiScanButton.type='button';wifiScanButton.className='icon-button remote-scan';
wifiScanButton.innerHTML=$('remoteScan').innerHTML;$('wifiConnectionState').after(wifiScanButton);
const wifiScanDialog=document.createElement('dialog');wifiScanDialog.id='wifiScanDialog';wifiScanDialog.className='content-dialog';
wifiScanDialog.innerHTML=`<div class="remote-dialog-head"><h3 id="wifiScanTitle"></h3><button id="wifiScanRefresh" type="button" class="icon-button remote-scan">${$('remoteSearchAgain').innerHTML}</button></div><p id="wifiScanStatus" class="context-status" role="status"></p><div id="wifiScanList" class="remote-devices"></div><form id="wifiScanForm" hidden><strong id="wifiChoiceName"></strong><p id="wifiChoiceSecurity" class="context-status"></p><label for="wifiScanPassword"><span id="wifiScanPasswordLabel"></span><input id="wifiScanPassword" type="password" autocomplete="new-password" maxlength="64"></label></form><div class="actions dialog-footer"><button id="wifiScanClose" type="button"></button><button id="wifiScanConnect" type="button" hidden></button></div>`;
document.body.append(wifiScanDialog);
function renderWifiScan(){
  if(wifiJoin&&wifiJoin.epoch!==epoch&&online){
    const target=mode==='ble'?bleDevice?.id:location.origin;
    if(wifiJoin.target===target)wifiJoin.epoch=epoch;
    else{wifiJoin=null;wifiScanState={...wifiScanState,scanState:'connectError'};}
  }
  if(wifiJoin&&performance.now()-wifiJoin.at>70000){wifiJoin=null;wifiScanState={...wifiScanState,scanState:'connectError'};}
  const blocked=!online||firmwareLocked()||devicePowerBlocked()||wifiPending||!!wifiState?.switching,joining=!!wifiJoin,scanning=wifiScanState?.scanState==='scanning';
  for(const [node,label] of [[wifiScanButton,'搜索 Wi-Fi'],[$('wifiScanRefresh'),'重新搜索']]){node.setAttribute('aria-label',tr(label));node.dataset.tooltip=tr(label);setControlDisabled(node,blocked||joining||scanning,wifiScanBusy);}
  wifiScanButton.disabled||=wifiState?.scanSupported!==true;
  $('wifiScanTitle').textContent=tr('搜索 Wi-Fi');$('wifiScanClose').textContent=tr('关闭');$('wifiScanConnect').textContent=tr(joining?'正在连接…':'连接');$('wifiScanPasswordLabel').textContent=tr('Wi-Fi 密码');
  wifiScanButton.classList.toggle('is-loading',scanning||wifiScanBusy);$('wifiScanRefresh').classList.toggle('is-loading',scanning||wifiScanBusy);
  wifiScanDialog.setAttribute('aria-busy',String(joining||scanning||wifiScanBusy));
  const phase=wifiJoin?.phase;
  $('wifiScanStatus').textContent=tr(phase==='connected'?'Wi-Fi 已连接':phase==='failed'||wifiScanState?.scanState==='connectError'?'Wi-Fi 连接失败，请检查密码和信号后重试。':phase==='dhcp'?'正在获取网络地址…':joining?'正在连接 Wi-Fi…':scanning||wifiScanBusy?'正在搜索…':wifiScanState?.scanState==='error'?'搜索失败，请稍后重试':wifiScanState?.networks?.length?'请选择要连接的 Wi-Fi（2.4 GHz）':'未找到 Wi-Fi，可刷新搜索或手动输入名称。');
  $('wifiScanStatus').dataset.loading=String(joining||scanning||wifiScanBusy);
  $('wifiScanList').hidden=!!wifiChoice||joining;$('wifiScanForm').hidden=!wifiChoice||!wifiChoice.encrypted;
  $('wifiScanPassword').disabled=blocked||joining;$('wifiScanConnect').hidden=!wifiChoice||!wifiChoice.encrypted;$('wifiScanConnect').disabled=blocked||joining;
  if(wifiChoice){$('wifiChoiceName').textContent=wifiChoice.ssid;$('wifiChoiceSecurity').textContent=wifiChoice.security;}
  const signature=JSON.stringify([wifiScanState?.networks,blocked,joining,uiLanguage]);
  if(signature!==wifiScanSignature){wifiScanSignature=signature;$('wifiScanList').replaceChildren();
    for(const network of [...wifiScanState?.networks||[]].sort((a,b)=>b.rssi-a.rssi)){
      const row=document.createElement('div');row.className='remote-device';const info=document.createElement('div'),name=document.createElement('strong'),detail=document.createElement('small'),button=document.createElement('button');
      name.textContent=network.ssid;detail.textContent=network.security==='Open'?tr('无密码'):network.security;info.append(name,detail,discoverySignal(network.rssi));
      button.textContent=tr(network.supported?'连接':'暂不支持');button.disabled=blocked||joining||!network.supported;
      button.onclick=()=>{wifiChoice=network;$('wifiScanPassword').value='';renderWifiScan();if(network.encrypted)$('wifiScanPassword').focus();else void connectScannedWifi();};
      row.append(info,button);$('wifiScanList').append(row);
    }
  }
}
async function searchWifi(){
  if(wifiScanBusy||wifiJoin||!online||wifiPending||firmwareLocked()||devicePowerBlocked())return;
  wifiScanBusy=true;wifiChoice=null;wifiScanState=null;$('wifiScanPassword').value='';const generation=++wifiScanGeneration,version=epoch;renderWifiScan();
  try{const result=await libraryRequest({op:'wifi',action:'scan'});if(version===epoch&&generation===wifiScanGeneration)wifiScanState=result;}
  catch(error){wifiScanState={scanState:'error'};const reason=userError(error);if(reason)message(reason,true);}
  finally{wifiScanBusy=false;renderWifiScan();}
}
async function connectScannedWifi(){
  if(!wifiChoice||wifiJoin||!online||wifiPending||firmwareLocked()||devicePowerBlocked())return;
  const choice=wifiChoice,password=choice.encrypted?$('wifiScanPassword').value:'';
  if(choice.encrypted&&!/^[\x20-\x7e]{8,63}$|^[0-9a-fA-F]{64}$/.test(password)){message(tr('请输入 8–63 位 Wi-Fi 密码；无密码网络请勾选相应选项'),true);return;}
  const version=epoch;
  wifiJoin={ssid:choice.ssid,at:performance.now(),epoch,target:mode==='ble'?bleDevice?.id:location.origin,phase:'connecting'};renderWifiScan();
  try{
    if(!await saveNetworkValues({ssid:choice.ssid,password})&&version===epoch&&online)throw Error(tr('设备忙，请稍后重试'));
    $('wifiSsid').value=choice.ssid;$('wifiOpen').checked=!choice.encrypted;$('wifiScanPassword').value='';
  }catch(error){if(version===epoch&&online){wifiJoin=null;const reason=userError(error);if(reason)message(reason,true);}}
  renderWifiScan();
}
async function pollWifiScan(){
  if(wifiScanReading||wifiScanBusy||wifiPending||document.hidden||!online||firmwareLocked())return;
  if(!wifiJoin&&(!wifiScanDialog.open||wifiScanState?.scanState!=='scanning'))return;
  const generation=wifiScanGeneration,version=epoch,join=wifiJoin;wifiScanReading=true;
  try{
    if(join){
      wifiPollAt=performance.now();const result=await wifiRequest();
      if(version!==epoch||wifiJoin!==join)return;
      renderWifi(result);join.phase=result.phase;
      if(result.connected&&!result.switching&&result.ssid===join.ssid){wifiJoin=null;wifiChoice=null;wifiScanDialog.close();message(tr('Wi-Fi 已连接'));}
      else if(result.status==='failed'||performance.now()-join.at>70000){wifiJoin=null;wifiScanState={...wifiScanState,scanState:'connectError'};message(tr('Wi-Fi 连接失败，请检查密码和信号后重试。'),true);}
    }else{
      const result=await libraryRequest({op:'wifi',action:'scanStatus'});
      if(version===epoch&&generation===wifiScanGeneration)wifiScanState=result;
    }
  }catch(error){if(join&&performance.now()-join.at>70000){wifiJoin=null;message(tr('设备连接已中断，请重新连接后检查网络状态。'),true);}}
  finally{wifiScanReading=false;renderWifiScan();}
}
wifiScanButton.onclick=()=>{if(wifiScanButton.disabled)return;wifiScanDialog.showModal();motionEnter(wifiScanDialog,'dialog');void searchWifi();};
$('wifiScanRefresh').onclick=()=>searchWifi();$('wifiScanClose').onclick=()=>wifiScanDialog.close();
$('wifiScanConnect').onclick=()=>connectScannedWifi();$('wifiScanForm').onsubmit=event=>{event.preventDefault();void connectScannedWifi();};
wifiScanDialog.addEventListener('close',()=>{$('wifiScanPassword').value='';});
setInterval(()=>{renderWifiScan();void pollWifiScan();},1000);queueMicrotask(renderWifiScan);
