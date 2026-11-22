// The P4 pairs with the remote directly. Browser Web Bluetooth is not required.
let remoteState=null,remoteBusy=false,remoteReadAt=0,remoteEpoch=-1,remoteWarned=null,remoteListSignature='';
const remoteCard=document.createElement('section');remoteCard.className='wifi';remoteCard.id='bluetoothRemoteCard';
remoteCard.innerHTML=`<div class="settings-card-head"><h3 class="settings-title"><span data-remote-label="蓝牙遥控器"></span><button id="remoteHelp" class="info-button" type="button" aria-label="说明"></button></h3><button id="remoteScan" type="button" class="icon-button remote-scan" aria-label="搜索遥控器" data-tooltip="搜索遥控器"><svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 4.5 4.5"/></svg></button></div>
<div class="wifi-body"><div class="remote-summary"><div><strong id="remoteName"></strong><p id="remoteStatus" class="context-status" role="status"></p></div><div id="remoteBattery" class="remote-battery" hidden><span class="remote-battery-track"><span id="remoteBatteryFill"></span></span><span id="remoteBatteryText"></span></div></div>
<p id="remoteBatteryWarning" class="context-status remote-battery-warning" hidden></p>
<div class="remote-options"><label class="check switch"><input id="remoteEnabled" type="checkbox" role="switch"><span data-remote-label="启用遥控器"></span></label>
<label class="check switch"><input id="remoteReconnect" type="checkbox" role="switch"><span data-remote-label="自动重新连接"></span></label></div>
<div class="remote-mappings"><div><label for="remoteSingle" data-remote-label="单击"></label><select id="remoteSingle"></select></div><div><label for="remoteDouble" data-remote-label="双击"></label><select id="remoteDouble"></select></div></div>
<div class="wifi-actions"><button id="remoteForget" type="button" class="ghost danger" data-remote-label="忘记遥控器"></button><button id="remoteDisconnect" type="button" class="ghost" data-remote-label="断开连接"></button></div></div>
<dialog id="remotePairDialog" class="content-dialog"><h3 data-remote-label="搜索遥控器"></h3><p id="remoteSearchState" class="context-status" role="status"></p><div id="remoteDevices" class="remote-devices"></div><div class="actions dialog-footer"><button id="remoteSearchAgain" type="button" class="ghost" data-remote-label="重新搜索"></button><button id="remotePairClose" type="button" data-remote-label="关闭"></button></div></dialog>`;
$('systemSettings').after(remoteCard);
for(const [id,key] of [['remoteSingle','singleClick'],['remoteDouble','doubleClick']]){
  for(const [value,label] of [[0,'无操作'],[1,'播放／暂停'],[2,'下一页／下一篇'],[3,'上一页／上一篇']]){
    const option=document.createElement('option');option.value=value;option.dataset.remoteLabel=label;$(id).append(option);
  }
  $(id).onchange=()=>remoteCommand('settings',{[key]:Number($(id).value)});
}
$('remoteEnabled').onchange=()=>remoteCommand('settings',{enabled:$('remoteEnabled').checked});
$('remoteReconnect').onchange=()=>remoteCommand('settings',{autoReconnect:$('remoteReconnect').checked});
$('remoteDisconnect').onclick=()=>remoteCommand('disconnect');
$('remoteForget').onclick=async()=>{if(await confirmAction(tr('忘记遥控器'),tr('将删除遥控器的配对信息，之后需要重新配对。')))await remoteCommand('forget',{confirmed:true});};
$('remotePairClose').onclick=()=>$('remotePairDialog').close();
$('remoteScan').onclick=()=>{if(!$('remotePairDialog').open){$('remotePairDialog').showModal();motionEnter($('remotePairDialog'),'dialog');}void remoteCommand('scan');};
$('remoteSearchAgain').onclick=()=>remoteCommand('scan');
const remoteErrorLabels={unsupported_hid_report:'暂不支持此遥控器的按键格式',not_hid_remote:'此设备不是受支持的蓝牙遥控器',remote_pair_failed:'配对失败，请先在原设备上忘记遥控器后重试',remote_connect_failed:'遥控器连接失败，请确认已开启且未连接其他设备',bluetooth_capacity:'蓝牙连接已满，请断开一个蓝牙网页后重试',remote_scan_failed:'搜索失败，请稍后重试',hid_discovery_failed:'读取遥控器服务失败',hid_map_failed:'读取遥控器按键格式失败',hid_reference_failed:'读取遥控器按键格式失败',hid_subscribe_failed:'订阅遥控器按键失败',settings_save_failed:'保存遥控器设置失败',pairing_code_unsupported:'此遥控器需要输入配对码，暂不支持'};
function renderRemote(){
  for(const node of remoteCard.querySelectorAll('[data-remote-label]'))node.textContent=tr(node.dataset.remoteLabel);
  $('remoteHelp').dataset.tooltip=tr('双击在整页模式翻页，在滚动模式切换稿件；切换稿件后保持暂停。单击约延迟0.35秒，以区分双击。长按暂不支持。')+'\n'+tr('请先断开遥控器与手机或电脑的连接，再搜索配对。遥控器占用一个蓝牙连接名额，与网页控制共存。电量不超过20%时提醒。');
  const s=remoteState,blocked=!online||firmwareLocked()||devicePowerBlocked();
  const labels={disabled:'未启用',idle:'未连接',disconnected:'未连接',scanning:'正在搜索…',connecting:'正在连接…',pairing:'正在配对…',discovering:'正在读取遥控器…',connected:'已连接',error:'连接异常'};
  $('remoteName').textContent=s?.name||tr('未配对遥控器');
  $('remoteStatus').textContent=!online?tr('设备未连接'):s?.errorDetail?tr(remoteErrorLabels[s.errorDetail]||'遥控器连接异常'):s?tr(labels[s.state]||'未连接'):tr('正在读取遥控器设置…');
  if(s){$('remoteEnabled').checked=s.enabled;$('remoteReconnect').checked=s.autoReconnect;$('remoteSingle').value=s.singleClick;$('remoteDouble').value=s.doubleClick;}
  const battery=s?.ready&&Number.isFinite(s.battery)&&s.battery>=0?s.battery:null,low=battery!==null&&battery<=20;
  $('remoteBattery').hidden=battery===null;$('remoteBattery').classList.toggle('low',low);
  if(battery!==null){$('remoteBatteryFill').style.width=battery+'%';$('remoteBatteryText').textContent=battery+'%';$('remoteBattery').setAttribute('aria-label',tr('遥控器电量')+' '+battery+'%');}
  $('remoteBatteryWarning').hidden=!low;$('remoteBatteryWarning').textContent=tr('遥控器电量较低，请及时充电。');
  const warningKey=epoch+':'+lastState?.boot+':'+s?.session;
  if(low&&online&&warningKey!==remoteWarned){remoteWarned=warningKey;message(tr('遥控器电量较低，请及时充电。'));}
  if(battery!==null&&battery>25)remoteWarned=null;
  const pending=s&&['connecting','pairing','discovering'].includes(s.state);
  for(const id of ['remoteEnabled','remoteReconnect','remoteSingle','remoteDouble'])setControlDisabled($(id),blocked||!s,remoteBusy);
  setControlDisabled($('remoteScan'),blocked||!s||s.connected||pending,remoteBusy||s?.scanning);
  setControlDisabled($('remoteDisconnect'),blocked||!s?.connected&&!pending,remoteBusy);
  setControlDisabled($('remoteForget'),blocked||!s?.paired&&!s?.connected,remoteBusy);
  setControlDisabled($('remoteSearchAgain'),blocked||!s||s.connected||pending,remoteBusy||s?.scanning);
  if(typeof renderCustomSelects==='function')renderCustomSelects();
  $('remoteScan').classList.toggle('is-loading',!!s?.scanning||remoteBusy);
  $('remoteSearchState').textContent=s?.scanning?tr('正在搜索…'):s?.devices?.length?tr('请选择要配对的遥控器'):tr('未找到遥控器，请确认已进入配对状态且未连接其他设备。');
  const signature=JSON.stringify([s?.devices,blocked,pending,s?.connected,remoteBusy,uiLanguage]);
  if(signature!==remoteListSignature){remoteListSignature=signature;$('remoteDevices').replaceChildren();
    for(const d of s?.devices||[]){const row=document.createElement('div');row.className='remote-device';const text=document.createElement('div'),name=document.createElement('strong'),address=document.createElement('small');name.textContent=d.name;address.textContent=d.address;text.append(name,address);const button=document.createElement('button');button.textContent=tr('连接');button.disabled=!!(blocked||pending||s.connected||remoteBusy);button.onclick=()=>remoteCommand('connect',{index:d.index});row.append(text,button);$('remoteDevices').append(row);}
  }
}
async function readRemote(force=false){
  if(!online||remoteBusy||firmwareLocked()||document.hidden||!force&&pageName!=='settings'&&remoteState&&!remoteState.paired)return;
  if(remoteEpoch!==epoch){remoteEpoch=epoch;remoteState=null;remoteReadAt=0;}
  if(!force&&performance.now()-remoteReadAt<(remoteState?.scanning||['connecting','pairing','discovering'].includes(remoteState?.state)?1200:pageName==='settings'?5000:30000))return;
  remoteReadAt=performance.now();const generation=epoch;
  try{const result=await libraryRequest({op:'remote',action:'status'});if(epoch===generation&&online){remoteState=result;renderRemote();}}
  catch(error){if(epoch===generation){remoteReadAt=performance.now()+10000;}}
}
async function remoteCommand(action,values={}){
  if(remoteBusy||!online||firmwareLocked()||devicePowerBlocked())return;
  remoteBusy=true;renderRemote();
  try{await libraryRequest({op:'remote',action,...values});await new Promise(resolve=>setTimeout(resolve,120));const result=await libraryRequest({op:'remote',action:'status'});remoteState=result;remoteReadAt=performance.now();if(action==='connect'&&result.ready&&$('remotePairDialog').open)$('remotePairDialog').close();}
  catch(error){const reason=userError(error);if(reason)message(reason,true);}
  finally{remoteBusy=false;renderRemote();}
}
setInterval(()=>{renderRemote();void readRemote();if(remoteState?.ready&&$('remotePairDialog').open)$('remotePairDialog').close();},1000);
queueMicrotask(()=>{mountCustomSelect('remoteSingle');mountCustomSelect('remoteDouble');renderRemote();});
