// Explicit device lifecycle, shared by library/editor/playback/settings.
// A missing response alone is never evidence that the device shut down.
function devicePowerPhase(){
  const lifecycle=lastState?.lifecycle;
  if(lifecycleIntent?.action==='power_on'&&performance.now()-lifecycleIntent.at<10000)return 'powering_on';
  if(['powered_off','powering_off','powering_on','shutting_down'].includes(lifecycle))return lifecycle;
  if(lifecycleIntent?.action==='hard_shutdown')return 'shutting_down';
  if(lifecycleIntent?.action==='shutdown'&&performance.now()-lifecycleIntent.at<10000)return lastState?.powerControl?'powering_off':'shutting_down';
  return '';
}
function devicePowerBlocked(){return !!devicePowerPhase();}
function renderDevicePower(){
  const phase=devicePowerPhase(),off=phase==='powered_off',starting=phase==='powering_on',hard=phase==='shutting_down';
  const pending=phase==='powering_off'||starting||hard;
  const toggle=$('deviceShutdown');
  toggle.classList.remove('danger');toggle.classList.toggle('primary',!off);
  toggle.dataset.power=off?'off':'on';toggle.setAttribute('aria-pressed',String(!off));
  setActionIcon(toggle,'shutdown',off?'开机':'关机');
  setControlDisabled(toggle,!online||pending||firmwareLocked(),busy||deviceActionConfirmPending);
  const notice=$('devicePowerNotice');
  statusBanner(notice,phase||null);
  if(!phase)return;
  const title=tr(off?'设备已关机':starting?'设备正在开机':hard?(online?'设备正在关机':'设备已关机'):'设备正在关机');
  const description=tr(hard?'请连接电源，并按设备的 RST 或重新供电开机。':
    !online?'设备连接已中断，重新连接后可开机；未保存的稿件仍保留在当前页面。':
    off?'屏幕已关闭，网络和蓝牙仍保持连接。点击开机即可继续使用。':
    starting?'正在恢复显示和设备设置，请稍候。':'正在关闭屏幕，已保存的稿件和设置将保留。');
  $('devicePowerText').textContent=title;
  $('devicePowerDescription').textContent=description;
  const progress=$('devicePowerProgress');progress.hidden=!starting;
  progress.value=lastState?.powerProgress??0;
  $('devicePowerPercent').hidden=!starting;$('devicePowerPercent').textContent=Math.max(0,Math.min(100,Math.floor(lastState?.powerProgress??0)))+'%';
  const button=$('devicePowerNoticeStart');button.hidden=!off;setControlDisabled(button,!online,busy);
}
$('devicePowerNoticeStart').onclick=()=>deviceAction('power_on');
