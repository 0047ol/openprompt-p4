// Per-tab recovery. Drafts stay in this tab's sessionStorage and never go to a server.
const tabSessionKey='teleprompter-tab-v1';
let tabSnapshot=null,tabRestorePending=false,tabRestoring=false;
try{const saved=JSON.parse(sessionStorage.getItem(tabSessionKey));if(saved?.version===1&&['home','settings','edit','play'].includes(saved.page))tabSnapshot=saved;}catch{}
function tabDevice(){return mode==='ble'?'ble:'+bleDevice?.id:wifiState?.mac||null;}
function saveTabSession(){
  if(!tabSessionReady||tabRestorePending||tabRestoring||pageTransition)return;
  const prior=tabSnapshot,device=tabDevice();
  const snapshot={version:1,page:pageName,transport:mode,device:device||prior?.device};
  if(pageName==='edit'&&editingDocument){
    snapshot.editor={doc:{...editingDocument,id:libraryState?.items.find(item=>item.title===editingDocument.title)?.id||editingDocument.id},title:$('documentTitle').value,text:$('script').value,dirty:documentDirty,conflict:documentConflict,start:$('script').selectionStart,end:$('script').selectionEnd,scroll:$('script').scrollTop};
  }
  if(pageName==='play'){
    const active=libraryState?.items.find(item=>item.title===libraryState.active);
    if(online&&active&&lastState&&!['starting','restarting','updating','resetting'].includes(lastState.lifecycle))snapshot.playback={title:active.title,id:active.id,text:previewMatchesDocument(previewLayout)?previewLayout.text:(prior?.playback&&prior.playback.id===active.id?prior.playback.text:null),backend:libraryState.storageBackend,boot:lastState.boot,mode:lastState.pageMode||0,position:lastState.pageMode?lastState.pageIndex:Math.round(lastState.scroll||0),owner:!playbackReadOnly()};
    else snapshot.playback=prior?.playback;
  }
  tabSnapshot=snapshot;
  try{sessionStorage.setItem(tabSessionKey,JSON.stringify(snapshot));if(mode==='ble'&&bleDevice?.id)sessionStorage.setItem('teleprompter-update-device',bleDevice.id);}catch{}
}
function resumeTabRoute(){
  const saved=tabSnapshot;if(!saved||saved.transport!==mode)return;
  tabRestorePending=saved.page==='play'||saved.page==='edit';
  if(saved.page==='edit'&&saved.editor){
    const e=saved.editor;editingDocument=e.doc;documentDirty=!!e.dirty;documentConflict=!!e.conflict;
    $('documentTitle').value=e.title;$('script').value=e.text;lastSent=e.doc.text;count();renderEditorHeading();
    $('draftState').textContent=tr(e.dirty?'未保存':'正在确认编辑权限，当前仅可预览。未保存的草稿仍保留。');
    requestAnimationFrame(()=>{$('script').setSelectionRange(e.start,e.end);$('script').scrollTop=e.scroll;});
  }
  showPage(saved.page);
}
async function restoreTabSession(){
  if(!tabRestorePending||tabRestoring||!online||!tabSnapshot||document.hidden||busy||libraryBusy||libraryActionPending||firmwareLocked()||devicePowerBlocked())return;
  const saved=tabSnapshot,version=epoch,boot=lastState?.boot;
  if(!lastState||libraryStateEpoch!==epoch||libraryState?.boot!==boot||!wifiState&&mode==='lan')return;
  const device=tabDevice();if(saved.device&&device&&saved.device!==device){tabRestorePending=false;return;}
  if(pageName!==saved.page){tabRestorePending=false;saveTabSession();return;}
  tabRestoring=true;
  let retry=false;
  const current=()=>epoch===version&&lastState?.boot===boot&&pageName===saved.page;
  try{
    if(saved.page==='edit'){
      const doc=editingDocument,id=doc?.id||saved.editor?.doc?.id,item=libraryState.items.find(item=>item.title===doc?.title&&item.id===id);
      if(doc?.storageBackend&&doc.storageBackend!==libraryState.storageBackend){documentConflict=true;return;}
      if(doc?.etag&&!item){documentConflict=true;return;}
      if(doc){editingDocument={...doc,storageBackend:libraryState.storageBackend,storageGeneration:libraryState.storageGeneration};if(doc.etag){releaseEditorLease();startEditorLease(editingDocument);await checkEditorLease();}}
      return;
    }
    const checkpoint=saved.playback;if(!checkpoint)return;
    const item=libraryState.items.find(item=>item.title===checkpoint.title&&item.id===checkpoint.id);
    if(!item||libraryState.storageBackend!==checkpoint.backend)return;
    await syncPlaybackPresence(true);
    if(!current()||playbackReadOnly())return;
    // Another controller's selected document and live progress always win.
    if(libraryState.active&&libraryState.active!==checkpoint.title)return;
    const restarted=boot!==checkpoint.boot;
    const remote=await libraryRequest({op:'get',title:item.title});
    if(!current()||checkpoint.text===null||checkpoint.text===undefined||remote.text!==checkpoint.text)return;
    if(!libraryState.active){
      if(!checkpoint.owner)return;
      const result=await libraryRequest({op:'select',title:checkpoint.title,restore:true,expected:remote.etag,id:item.id});
      if(!current())return;
      renderLibrary({...libraryState,active:result.active||checkpoint.title,revision:result.revision});
      render(await readStatus());
    }
    if(!restarted||!checkpoint.owner||lastState.playing||lastState.countdownRemainingMs||lastState.scroll>0||lastState.pageIndex>0||lastState.playbackElapsedMs>0||lastState.pageMode!==checkpoint.mode)return;
    if(checkpoint.position>0&&Number.isFinite(lastState.seekRevision)){
      // Read layout directly: preview loading waits until recovery finishes.
      let layout,state;
      for(let attempt=0;attempt<8;attempt++){
        try{layout=await libraryRequest({op:'preview'});}
        catch(error){if(error.message!=='preview_not_ready')throw error;await new Promise(resolve=>setTimeout(resolve,150));continue;}
        if(!current())return;
        state=await readStatus();if(!current()||state.playing||state.scroll>0||state.pageIndex>0||state.playbackElapsedMs>0||state.scriptRevision!==lastState.scriptRevision)return;
        if(layout.revision===state.previewRevision&&(!checkpoint.mode||state.pageCount>0))break;
        await new Promise(resolve=>setTimeout(resolve,150));
      }
      if(!layout||!state||layout.revision!==state.previewRevision||checkpoint.mode&&!state.pageCount){retry=true;return;}
      let position=checkpoint.mode?Math.min(checkpoint.position,state.pageCount-1):Math.min(checkpoint.position,Math.max(0,(layout.endScroll||0)-1));
      if(position>0)render(await applyControl({seek:position,seekScript:state.scriptRevision,seekState:state.revision}));
    }
  }catch(error){retry=error.message==='preview_not_ready'||['NetworkError','TimeoutError'].includes(error.name);const reason=retry?null:userError(error);if(reason&&current())message(reason,true);}
  finally{tabRestoring=false;if(current()&&!retry){tabRestorePending=false;playbackRestoreHint=null;renderEditorConflict();renderPlaybackDocument();saveTabSession();preview();}}
}
const renderWithTabRecovery=render;
render=function(state){
  const rebooted=lastState?.boot!==undefined&&state.boot!==lastState.boot;
  if(rebooted){saveTabSession();if(['play','edit'].includes(pageName))tabRestorePending=true;}
  renderWithTabRecovery(state);
};
resumeTabRoute();tabSessionReady=true;
setInterval(()=>{if(tabRestorePending)void restoreTabSession();else saveTabSession();},750);
for(const event of ['pagehide','beforeunload'])window.addEventListener(event,saveTabSession);
document.addEventListener('input',()=>{if(pageName==='edit')saveTabSession();});
