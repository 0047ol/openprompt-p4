// Fixed fault summaries only; no manuscript contents or network credentials.
let diagnosticsLoading=false,diagnosticsLast=null,diagnosticsBoot=null,diagnosticsEpoch=null;
function renderCrashRecovery(){
  const supported=Number.isFinite(lastState?.crashCount);
  $('diagnosticsCard').hidden=!supported;
  if($('diagnosticsDialog').open&&diagnosticsBoot!==lastState?.boot)$('diagnosticsDialog').close();
  statusBanner($('crashRecoveryNotice'),online&&lastState?.crashPending?String(lastState.lastCrashId):null);
  $('diagnosticsSummary').textContent=tr((lastState?.crashCount??0)>0?'设备保留了最近的异常重启记录。':'暂无异常重启记录。');
  for(const id of ['diagnosticsOpen','diagnosticsExport','crashRecoveryOpen','diagnosticsClear'])setControlDisabled($(id),!online,diagnosticsLoading);
  setControlDisabled($('diagnosticsClear'),!online||!diagnosticsLast?.records?.length,diagnosticsLoading);
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
  const dialog=$('diagnosticsDialog');if(!dialog.open){dialog.showModal();motionEnter(dialog,'dialog');}
}
$('diagnosticsOpen').onclick=$('crashRecoveryOpen').onclick=showDiagnostics;
$('diagnosticsExport').onclick=async()=>{
  const result=await readDiagnostics();if(!result)return;
  const fields=['id','version','reason','resetReason','panicCaptured','uptimeLowerBoundMs','core','exception','pc','returnAddress','stackPointer','machineCause','faultAddress','possibleCodeAddresses','stages'];
  const records=(result.records||[]).map(record=>Object.fromEntries(fields.filter(key=>Object.hasOwn(record,key)).map(key=>[key,record[key]])));
  const data={format:'openprompt-p4-diagnostics-v1',exportedAt:new Date().toISOString(),records};
  const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)+'\n'],{type:'application/json'}));
  const link=document.createElement('a');link.href=url;link.download='openprompt-p4-diagnostics-'+new Date().toISOString().replace(/[:.]/g,'-')+'.json';document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);
};
$('diagnosticsClose').onclick=async()=>{
  if(!diagnosticsLast?.pending){$('diagnosticsDialog').close();return;}
  await changeDiagnostics('acknowledge');
};
$('diagnosticsClear').onclick=async()=>{
  if(!online||diagnosticsLoading)return;
  const boot=diagnosticsBoot,generation=epoch,id=diagnosticsLast?.lastId;
  if(!await confirmAction(tr('清空故障记录？'),tr('仅删除异常重启记录，不影响稿件、设备设置和网络配置。'),{danger:true}))return;
  if(boot!==lastState?.boot||generation!==epoch||id!==diagnosticsLast?.lastId)return;
  await changeDiagnostics('clear');
};
async function changeDiagnostics(action){
  if(!diagnosticsLast||!online||diagnosticsLoading)return;
  if(diagnosticsBoot!==lastState?.boot||diagnosticsEpoch!==epoch){void showDiagnostics();return;}
  diagnosticsLoading=true;renderCrashRecovery();
  try{
    const result=await libraryRequest({op:'diagnostics',action,id:diagnosticsLast.lastId,confirmed:action==='clear'});
    if(diagnosticsBoot!==lastState?.boot||diagnosticsEpoch!==epoch)return;
    diagnosticsLast=result;
    if(lastState?.lastCrashId===result.lastId)lastState={...lastState,crashPending:false,...(action==='clear'?{crashCount:0}:{})};
    if(action==='clear'){$('diagnosticsRecords').replaceChildren();$('diagnosticsEmpty').hidden=false;}
    else $('diagnosticsDialog').close();
  }catch(error){const reason=userError(error);if(reason)message(reason,true);}
  finally{diagnosticsLoading=false;renderCrashRecovery();}
}
