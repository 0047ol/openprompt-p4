// Fixed fault summaries only; no manuscript contents or network credentials.
let diagnosticsLoading=false,diagnosticsLast=null,diagnosticsBoot=null,diagnosticsEpoch=null;
function renderCrashRecovery(){
  const supported=Number.isFinite(lastState?.crashCount);
  $('diagnosticsCard').hidden=!supported;
  if($('diagnosticsDialog').open&&diagnosticsBoot!==lastState?.boot)$('diagnosticsDialog').close();
  statusBanner($('crashRecoveryNotice'),online&&lastState?.crashPending?String(lastState.lastCrashId):null);
  $('diagnosticsSummary').textContent=tr((lastState?.crashCount??0)>0?'设备保留了最近的异常重启记录。':'暂无异常重启记录。');
  for(const id of ['diagnosticsOpen','diagnosticsDownload','crashRecoveryOpen'])setControlDisabled($(id),!online,diagnosticsLoading);
}
async function readDiagnostics(){
  if(diagnosticsLoading)return null;
  diagnosticsLoading=true;renderCrashRecovery();
  try{
    const boot=lastState?.boot,generation=epoch,result=await libraryRequest({op:'diagnostics',action:'status'});
    if(boot!==lastState?.boot||generation!==epoch||!online)return null;
    diagnosticsLast=result;diagnosticsBoot=boot;diagnosticsEpoch=generation;return result;
  }catch(error){const reason=userError(error);if(reason)message(reason,true);return null;}
  finally{diagnosticsLoading=false;renderCrashRecovery();}
}
function diagnosticsDescription(record){
  const reason={panic:'程序异常',interrupt_watchdog:'中断看门狗',task_watchdog:'任务看门狗',watchdog:'看门狗复位',brownout:'供电电压不足'};
  return `${tr('记录')} ${record.id} · ${tr(reason[record.reason]||'未知复位原因')} · ${record.version}`;
}
async function showDiagnostics(){
  const result=await readDiagnostics();if(!result)return;
  const list=$('diagnosticsRecords');list.replaceChildren();
  for(const record of result.records??[]){
    const item=document.createElement('li'),title=document.createElement('strong'),detail=document.createElement('p');
    title.textContent=diagnosticsDescription(record);
    detail.textContent=record.panicCaptured?`${tr('故障地址')} ${record.pc} · ${tr('核心')} ${record.core}`:tr('未捕获详细现场，已记录复位原因。');
    item.append(title,detail);list.append(item);
  }
  $('diagnosticsEmpty').hidden=!!result.records?.length;
  $('diagnosticsAcknowledge').hidden=!result.pending;
  $('diagnosticsClose').hidden=!!result.pending;
  const dialog=$('diagnosticsDialog');if(!dialog.open){dialog.showModal();motionEnter(dialog,'dialog');}
}
async function downloadDiagnostics(){
  const result=await readDiagnostics();if(!result)return;
  const blob=new Blob([JSON.stringify({format:'openprompt-p4-diagnostics-v1',exportedAt:new Date().toISOString(),...result},null,2)+'\n'],{type:'application/json'});
  const url=URL.createObjectURL(blob),link=document.createElement('a');
  link.href=url;link.download=`openprompt-p4-diagnostics-${result.lastId??0}.json`;link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
}
$('diagnosticsOpen').onclick=$('crashRecoveryOpen').onclick=showDiagnostics;
$('diagnosticsDownload').onclick=$('diagnosticsDialogDownload').onclick=downloadDiagnostics;
$('diagnosticsClose').onclick=()=>$('diagnosticsDialog').close();
$('diagnosticsAcknowledge').onclick=async()=>{
  if(!diagnosticsLast||!online||diagnosticsLoading)return;
  if(diagnosticsBoot!==lastState?.boot||diagnosticsEpoch!==epoch){void showDiagnostics();return;}
  diagnosticsLoading=true;renderCrashRecovery();
  try{
    await libraryRequest({op:'diagnostics',action:'acknowledge',id:diagnosticsLast.lastId});
    if(lastState?.lastCrashId===diagnosticsLast.lastId)lastState={...lastState,crashPending:false};
    $('diagnosticsDialog').close();
  }catch(error){const reason=userError(error);if(reason)message(reason,true);}
  finally{diagnosticsLoading=false;renderCrashRecovery();}
};
