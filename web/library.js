/* Device SD/internal library over HTTP or chunked BLE. No browser persistence. */
let pageName='home',libraryState=null,editingDocument=null,documentDirty=false,documentConflict=false;
let editorLease=null,editorLeaseChecking=false;
let playbackAccess={epoch:-1,allowed:false,at:0};
function playbackReadOnly(){return pageName==='play'&&deviceSettings?.multiControl!==undefined&&(playbackAccess.epoch!==epoch||!playbackAccess.allowed||performance.now()-playbackAccess.at>8000);}
function renderPlaybackAccess(){
  const readOnly=playbackReadOnly();
  if(readOnly)document.querySelectorAll('#playPage [data-device],#playEdit').forEach(el=>setControlDisabled(el,true));
  statusBanner($('playbackLeaseNotice'),pageName==='play'&&readOnly?'readonly':null);
}
function editorReadOnly(){return !!editingDocument?.etag&&(!editorLease?.owned||editorLease.epoch!==epoch||performance.now()>=editorLease.until);}
function renderEditorLease(){
  const readOnly=editorReadOnly();$('script').readOnly=readOnly;
  for(const id of ['import','clear','send'])setControlDisabled($(id),readOnly||!online||firmwareLocked()||devicePowerBlocked()||(id==='send'&&encoder.encode($('script').value).length>8192),busy||libraryActionPending);
  setControlDisabled($('editorPrompt'),!editingDocument||!online||firmwareLocked()||devicePowerBlocked()||(documentDirty&&readOnly),busy||libraryActionPending);
  statusBanner($('editorLeaseNotice'),pageName==='edit'&&readOnly?'locked':null);
  if(pageName==='edit'&&readOnly)$('editorLeaseText').textContent=tr(editorLease?.occupied?'其他控制端正在编辑此稿件，当前仅可预览。对方保存或退出后将自动恢复编辑。':'正在确认编辑权限，当前仅可预览。未保存的草稿仍保留。');
}
function editorLeaseRequest(lease,action){return {op:'editLease',action,title:lease.title,editClient:lease.client,storageBackend:lease.storageBackend,storageGeneration:lease.storageGeneration};}
function releaseEditorLease(leaving=false){
  const lease=editorLease;editorLease=null;if(!lease)return;
  lease.released=true;
  if(lease.epoch!==epoch||!online)return;
  const request=editorLeaseRequest(lease,'release');
  if(leaving&&mode==='lan'){fetch('/api/library',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(request),keepalive:true}).catch(()=>{});}
  else void libraryRequest(request).catch(()=>{});
}
async function checkEditorLease(){
  if(editorLease&&editorLease.epoch!==epoch&&online&&libraryStateEpoch===epoch)startEditorLease(editingDocument);
  const lease=editorLease;
  if(editorLeaseChecking||!lease||lease.released||pageName!=='edit'||!editingDocument?.etag||!online||document.hidden||firmwareLocked()||devicePowerBlocked()||libraryActionPending)return;
  editorLeaseChecking=true;
  const current=()=>editorLease===lease&&!lease.released&&lease.epoch===epoch&&pageName==='edit';
  try{
    const wasOwned=lease.owned&&performance.now()<lease.until;
    const result=await libraryRequest(editorLeaseRequest(lease,wasOwned?'renew':'acquire'),current);
    if(!current())return;
    lease.owned=false;lease.occupied=result.occupied;
    if(result.owned){
      if(result.etag!==editingDocument.etag){
        if(documentDirty){documentConflict=true;$('draftState').textContent=tr('其他端已修改，请重新读取');}
        else{
          const remote=await libraryRequest({op:'get',title:lease.title,storageBackend:lease.storageBackend,storageGeneration:lease.storageGeneration},current);
          if(!current())return;
          editingDocument=remote;$('script').value=remote.text;lastSent=remote.text;count();
        }
      }
      lease.owned=true;lease.until=performance.now()+Math.max(0,result.leaseMs-5000);
    }
  }catch{if(current()){lease.owned=false;lease.occupied=false;}}
  finally{editorLeaseChecking=false;renderEditorLease();renderEditorConflict();if(editorLease&&editorLease!==lease)void checkEditorLease();}
}
setInterval(()=>{renderEditorLease();void checkEditorLease();},3000);
window.addEventListener('pagehide',()=>releaseEditorLease(true));
window.addEventListener('pageshow',()=>{if(pageName==='edit'&&editingDocument?.etag&&!editorLease)startEditorLease(editingDocument);void checkEditorLease();});
document.addEventListener('visibilitychange',()=>{if(!document.hidden)void checkEditorLease();});
function startEditorLease(doc){
  editorLease={title:doc.title,client:Array.from(crypto.getRandomValues(new Uint8Array(16)),b=>b.toString(16).padStart(2,'0')).join(''),storageBackend:doc.storageBackend,storageGeneration:doc.storageGeneration,epoch,owned:false,until:0};
  renderEditorLease();
}
let activeDocument=null,libraryPollAt=0,libraryBusy=false,libraryRenderKey='';
let libraryStateEpoch=-1,finishPlaybackSync;
let libraryActionPending=false;
const libraryFileActions=new Map();
let pageTransition=null;
let libraryStorageFailure='';
let libraryStorageFallback=false;
let libraryView={layout:0,sort:6,boot:null,revision:-1};
let librarySelectionMode=false,librarySelection=new Set(),libraryViewPending=false;
let librarySortable=null,libraryDragging=false,libraryOrderPending=false,libraryRefreshDeferred=false;
let librarySortEditing=false;
let librarySortSaved=null;
let libraryLayoutMotion=null;
function finishLibraryLayoutMotion(){
  const current=libraryLayoutMotion;libraryLayoutMotion=null;
  if(current)for(const job of current.jobs)job?.cancel();
  delete $('libraryList').dataset.layoutChanging;
}
function setLibraryLayout(layout){
  const list=$('libraryList');if(list.dataset.layout===layout)return;
  const animate=!!list.dataset.layout&&!!list.querySelector('.document-card')&&pageName==='home'&&!document.hidden&&!libraryDragging&&!list.classList.contains('region-loading')&&motionEnabled();
  finishLibraryLayoutMotion();
  // Animate only the live layout. Overlapping old/new translucent copies made
  // text and borders pulse, especially when cards replaced a full-width list.
  // Keep the container at its natural new height; interpolating the old card
  // height leaves an empty tail below compact rows until the animation ends.
  list.dataset.layout=layout;
  if(!animate)return;
  list.dataset.layoutChanging='true';
  const current={jobs:[]};libraryLayoutMotion=current;
  const incoming=motionAnimate(list,[{transform:'translateX(6px)'},{transform:'none'}],{duration:180});
  current.jobs.push(incoming);
  const finish=()=>{if(libraryLayoutMotion===current)finishLibraryLayoutMotion();};
  if(incoming)incoming.finished.then(finish,finish);else finish();
}
let libraryDragY=null,libraryDragFrame=0,libraryDragAt=0;
function trackLibraryDrag(event){
  const point=event.touches?.[0]||event;
  if(Number.isFinite(point.clientY))libraryDragY=point.clientY;
}
document.addEventListener('pointermove',trackLibraryDrag,{passive:true});
document.addEventListener('touchmove',trackLibraryDrag,{passive:true});
function scrollLibraryDrag(now){
  if(!libraryDragging){libraryDragFrame=0;return;}
  const dt=Math.min(40,libraryDragAt?now-libraryDragAt:16);libraryDragAt=now;
  const edge=80,top=document.querySelector('.library-header')?.getBoundingClientRect().bottom||0;
  const down=Math.max(0,Math.min(1,(libraryDragY-(innerHeight-edge))/edge));
  const up=Math.max(0,Math.min(1,(top+edge-libraryDragY)/edge));
  if(libraryDragY!==null&&(down||up)){
    window.scrollBy(0,(down-up)*900*dt/1000);
    if(down)checkLibraryScroll();
  }
  libraryDragFrame=requestAnimationFrame(scrollLibraryDrag);
}
let playbackRestoreHint=null;
let librarySearchResult=null,librarySearchTimer,librarySearchGeneration=0,finishSearchLoading;
const LIBRARY_PAGE_SIZE=24;
let libraryPageLimit=LIBRARY_PAGE_SIZE,libraryPageGeneration=0,libraryPagePending=false,libraryPageError='';
const libraryDetails=new Map();
const libraryMore=document.createElement('div');libraryMore.className='library-more';
libraryMore.setAttribute('role','status');libraryMore.setAttribute('aria-live','polite');
$('libraryList').after(libraryMore);
function matchingLibraryItems(){
  const query=$('librarySearch').value.trim().toLowerCase();
  if(query&&librarySearchResult?.query===query)return orderedLibraryItems(librarySearchResult.items);
  const terms=query.split(/\s+/).filter(Boolean);
  return orderedLibraryItems().filter(item=>terms.every(term=>(item.title+' '+(item.excerpt||'')).toLowerCase().includes(term)));
}
function updateLibraryMore(){
  libraryMore.replaceChildren();
  const more=matchingLibraryItems().length>libraryPageLimit;
  libraryMore.hidden=!more&&!libraryPagePending&&!libraryPageError;
  libraryMore.setAttribute('aria-busy',String(libraryPagePending));
  if(libraryPagePending){const text=document.createElement('span');text.className='library-more-loading';text.textContent=tr('正在加载更多稿件…');libraryMore.append(text);}
  else if(libraryPageError){const retry=document.createElement('button');retry.textContent=tr('加载失败，点击重试');retry.onclick=()=>void loadMoreLibrary();libraryMore.append(retry);}
  else if(more)libraryMore.textContent=tr('继续向下滚动加载');
}
function checkLibraryScroll(){
  if(!libraryMore.hidden&&!libraryPageError&&libraryMore.getBoundingClientRect().top<innerHeight+320)void loadMoreLibrary();
}
window.addEventListener('scroll',checkLibraryScroll,{passive:true});
window.addEventListener('resize',checkLibraryScroll,{passive:true});
async function fillLibraryDetails(items,current){
  if(!libraryState?.compact)return;
  const missing=items.filter(item=>!libraryDetails.has(item.title));
  for(let offset=0;offset<missing.length;offset+=LIBRARY_PAGE_SIZE){
    const result=await libraryRequest({op:'list',titles:missing.slice(offset,offset+LIBRARY_PAGE_SIZE).map(item=>item.title)},current);
    if(!current())return;
    if(result.boot!==libraryState.boot||result.revision!==libraryState.revision||result.orderEtag!==libraryState.orderEtag)throw Error(tr('稿件列表已变化，请刷新后重新排序'));
    for(const item of result.items)libraryDetails.set(item.title,item);
  }
}
async function loadMoreLibrary(){
  if(pageName!=='home'||!online||libraryPagePending||libraryBusy||libraryActionPending||firmwareLocked()||finishSearchLoading||$('libraryList').classList.contains('region-loading'))return;
  const items=matchingLibraryItems();if(libraryPageLimit>=items.length)return;
  const generation=libraryPageGeneration,version=epoch;
  const current=()=>generation===libraryPageGeneration&&version===epoch;
  libraryPagePending=true;libraryPageError='';updateLibraryMore();
  try{
    const end=Math.min(items.length,libraryPageLimit+LIBRARY_PAGE_SIZE);
    await fillLibraryDetails(items.slice(libraryPageLimit,end),current);
    if(current()){libraryPageLimit=end;libraryRenderKey='';renderLibrary(libraryState,false,true);}
  }catch(error){if(current())libraryPageError=userError(error);}
  finally{if(current()){libraryPagePending=false;updateLibraryMore();if(!libraryPageError)requestAnimationFrame(checkLibraryScroll);}}
}
const libraryMoreObserver=new IntersectionObserver(entries=>{
  if(entries.some(entry=>entry.isIntersecting)&&!libraryPageError)void loadMoreLibrary();
},{rootMargin:'320px'});
libraryMoreObserver.observe(libraryMore);
// Match the device's UTF-8/numeric ordering so automatic next and the badges agree.
const libraryTitleOrder={compare(a,b){
  const aa=encoder.encode(a),bb=encoder.encode(b);let x=0,y=0;
  const digit=v=>v>=48&&v<=57;
  while(x<aa.length&&y<bb.length){
    if(digit(aa[x])&&digit(bb[y])){
      let ae=x,be=y;while(digit(aa[ae]))ae++;while(digit(bb[be]))be++;
      while(aa[x]===48&&x+1<ae)x++;while(bb[y]===48&&y+1<be)y++;
      if(ae-x!==be-y)return ae-x<be-y?-1:1;
      while(x<ae){if(aa[x]!==bb[y])return aa[x]-bb[y];x++;y++;}continue;
    }
    if(aa[x]!==bb[y])return aa[x]-bb[y];x++;y++;
  }
  return Number(x<aa.length)-Number(y<bb.length);
}};
const librarySortLabels=['名称升序','名称降序','更新：最新在前','更新：最早在前','创建：最新在前','创建：最早在前','手动排序'];
function librarySortAscending(sort=libraryView.sort){return sort<2?sort===0:sort%2===1;}
function librarySortValue(field,ascending){return field*2+(field===0?!ascending:ascending);}
function orderedLibraryItems(items=libraryState?.items||[]){
  const byTitle=(a,b)=>libraryTitleOrder.compare(a.title,b.title)||(a.title<b.title?-1:a.title>b.title?1:0);
  return [...items].sort((a,b)=>{
    if(libraryView.sort===6){
      const ar=Number.isInteger(a.order)&&a.order>=0?a.order:Infinity,br=Number.isInteger(b.order)&&b.order>=0?b.order:Infinity;
      if(ar!==br)return ar<br?-1:1;
      return (a.createdAt||0)-(b.createdAt||0)||byTitle(a,b);
    }
    if(libraryView.sort<2)return byTitle(a,b)*(libraryView.sort===1?-1:1);
    const key=libraryView.sort<4?'updatedAt':'createdAt',av=a[key],bv=b[key];
    const valid=value=>Number.isFinite(value)&&value>0;
    if(valid(av)!==valid(bv))return valid(av)?-1:1;
    return (valid(av)?(av-bv)*(libraryView.sort%2?1:-1):0)||byTitle(a,b);
  });
}
function applyLibraryView(settings,revision=settings.revision,hydrate=true){
  if(libraryViewPending)return;
  if(settings.boot===libraryView.boot&&Number.isFinite(revision)&&revision<libraryView.revision)return;
  const layout=settings.libraryLayout,sort=settings.librarySort;
  if(![0,1].includes(layout)&&!(Number.isInteger(sort)&&sort>=0&&sort<=6))return;
  const changed=layout!==undefined&&layout!==libraryView.layout||sort!==undefined&&sort!==libraryView.sort;
  libraryView={layout:[0,1].includes(layout)?layout:libraryView.layout,sort:Number.isInteger(sort)&&sort>=0&&sort<=6?sort:libraryView.sort,boot:settings.boot,revision:revision??libraryView.revision};
  if(libraryView.sort!==6)librarySortEditing=false;
  if(changed&&libraryState){libraryRenderKey='';renderLibrary(libraryState,false);if(hydrate)void prepareLibraryPage().catch(error=>{if(error.message!=='preview_superseded')message(error,true);});}
  updateLibraryToolbar();
}
function visibleLibraryItems(){
  return matchingLibraryItems().slice(0,libraryPageLimit).map(item=>libraryDetails.get(item.title)||item);
}
async function searchLibrary(){
  clearTimeout(librarySearchTimer);const query=$('librarySearch').value.trim().toLowerCase(),generation=++librarySearchGeneration,version=epoch;
  const finish=()=>{if(generation===librarySearchGeneration){finishSearchLoading?.();finishSearchLoading=null;}};
  const current=()=>version===epoch&&generation===librarySearchGeneration;
  if(!query){
    librarySearchResult=null;libraryPageLimit=LIBRARY_PAGE_SIZE;
    try{await prepareLibraryPage(current);}finally{finish();}
    return;
  }
  if(!online){finish();return;}
  finishSearchLoading ||= beginRegionLoading($('libraryList'),'正在搜索稿件…');
  try{
    const result=await listAll('list',query,current,true);
    if(!current())return;
    librarySearchResult={query,items:result.items};libraryPageLimit=LIBRARY_PAGE_SIZE;
    await prepareLibraryPage(current);
  }catch(error){if(generation===librarySearchGeneration&&version===epoch)message(error,true);}
  finally{finish();}
}
function updateLibraryToolbar(){
  const unavailable=firmwareLocked()||!online||!libraryState?.mounted;
  const pending=busy||libraryActionPending||libraryOrderPending;
  const locked=unavailable||pending;
  // Saving a display preference does not invalidate manuscript actions.
  for(const id of ['libraryLayout','librarySort'])setControlDisabled($(id),unavailable,pending||libraryViewPending);
  const sorting=librarySortEditing&&libraryView.sort===6;
  document.body.dataset.sorting=String(sorting);
  $('librarySort').hidden=librarySelectionMode||sorting;
  $('librarySort').closest('.library-sort-wrap').hidden=librarySelectionMode||sorting;
  $('libraryManualSort').hidden=librarySelectionMode||sorting;
  setControlDisabled($('libraryManualSort'),unavailable,pending||libraryViewPending||libraryFileActions.size>0);
  $('libraryManualSort').setAttribute('aria-pressed',String(sorting));
  $('libraryManualSort').dataset.tooltip=tr('手动排序');
  $('librarySortExit').hidden=!sorting;
  setControlDisabled($('librarySortExit'),false,pending||libraryViewPending||libraryDragging);
  $('librarySelectAllWrap').hidden=sorting;
  document.querySelector('.library-browse').hidden=sorting;
  const headingActions=document.querySelector('.library-heading .buttons');
  headingActions.style.visibility=sorting?'hidden':'';headingActions.inert=sorting;
  $('libraryLayout').hidden=$('libraryNew').hidden=librarySelectionMode||sorting;
  for(const id of ['openSettings','libraryRefresh','libraryNew'])setControlDisabled($(id),unavailable,pending);
  $('librarySort').setAttribute('aria-pressed','false');
  const target=libraryView.layout?'切换为卡片':'切换为列表';
  if($('libraryLayout').dataset.actionLabel!==target)setActionIcon($('libraryLayout'),libraryView.layout?'grid':'list',target);
  $('librarySort').dataset.tooltip=tr('排序决定上一稿和下一稿的顺序')+' · '+tr(librarySortLabels[libraryView.sort]);
  for(const item of $('librarySortMenu').querySelectorAll('[data-sort-field]'))item.setAttribute('aria-checked',String(libraryView.sort<6&&Number(item.dataset.sortField)===Math.floor(libraryView.sort/2)));
  for(const item of $('librarySortMenu').querySelectorAll('[data-sort-direction]')){
    item.disabled=libraryView.sort===6;
    item.setAttribute('aria-checked',String(libraryView.sort<6&&(item.dataset.sortDirection==='asc')===librarySortAscending()));
  }
  if(locked)closeLibrarySort();
  setLibraryLayout(libraryView.layout?'list':'cards');
  $('libraryList').dataset.selecting=String(librarySelectionMode);
  $('libraryList').dataset.manual=String(sorting);
  for(const handle of $('libraryList').querySelectorAll('.document-drag'))setControlDisabled(handle,unavailable||!!$('librarySearch').value.trim(),pending||libraryViewPending);
  librarySortable?.option('disabled',locked||libraryViewPending||!sorting||!!$('librarySearch').value.trim());
  $('librarySelectionCount').hidden=!librarySelectionMode;$('libraryDeleteSelected').hidden=!librarySelectionMode||sorting;
  setControlDisabled($('libraryDeleteSelected'),unavailable||!librarySelection.size,pending);
  syncLibraryFileActions();
  const visible=matchingLibraryItems().filter(item=>!libraryFileActions.has(item.title)),checked=visible.filter(item=>librarySelection.has(item.title)).length;
  $('librarySelectAll').checked=visible.length>0&&checked===visible.length;
  $('librarySelectAll').indeterminate=checked>0&&checked<visible.length;
  setControlDisabled($('librarySelectAll'),unavailable||!visible.length,pending);
  $('librarySelectionCount').textContent=librarySelection.size+' '+tr('项已选');
  for(const card of $('libraryList').querySelectorAll('.document-card[data-document-title]')){
    const selected=librarySelection.has(card.dataset.documentTitle),input=card.querySelector('.document-select input');
    const filePending=libraryFileActions.has(card.dataset.documentTitle);
    card.dataset.selected=String(selected);
    if(input){input.checked=selected;setControlDisabled(input,unavailable,pending||filePending);}
    for(const button of card.querySelectorAll('.document-actions button'))setControlDisabled(button,unavailable||sorting||librarySelectionMode,pending||filePending);
    if(librarySelectionMode){card.tabIndex=0;card.setAttribute('role','checkbox');card.setAttribute('aria-checked',String(selected));}
    else{card.removeAttribute('tabindex');card.removeAttribute('role');card.removeAttribute('aria-checked');}
  }
}
function toggleCardSelection(event){
  const card=event.target.closest('.document-card[data-document-title]');
  if(!librarySelectionMode||!card)return;
  if(event.type==='keydown'&&![' ','Enter'].includes(event.key))return;
  const input=card.querySelector('.document-select input');if(input?.disabled)return;
  if(event.type==='click'&&event.target===input){event.stopPropagation();return;}
  event.preventDefault();event.stopImmediatePropagation();
  const title=card.dataset.documentTitle;
  if(librarySelection.has(title))librarySelection.delete(title);else librarySelection.add(title);
  updateLibraryToolbar();
}
$('libraryList').addEventListener('click',toggleCardSelection,true);
$('libraryList').addEventListener('keydown',toggleCardSelection,true);
function closeLibrarySort(){
  $('librarySortMenu').hidden=true;$('librarySort').setAttribute('aria-expanded','false');
}
async function saveLibraryView(patch){
  if(libraryViewPending||!online||busy||libraryActionPending)return;
  const version=epoch,previous={...libraryView};
  libraryViewPending=true;closeLibrarySort();
  if(patch.libraryLayout!==undefined)libraryView.layout=patch.libraryLayout;
  if(patch.librarySort!==undefined)libraryView.sort=patch.librarySort;
  libraryRenderKey='';updateLibraryToolbar();
  try{
    if(libraryState)await prepareLibraryPage();
    const operation=queue.then(async()=>{
      if(version!==epoch)throw new DOMException('Connection changed','AbortError');
      await writeDeviceSettings(patch);return readDeviceSettings();
    });
    queue=operation.catch(()=>{});
    const settings=await operation;
    if(version===epoch){libraryViewPending=false;renderDeviceSettings(settings);}
  }catch(error){
    if(version===epoch){libraryView=previous;libraryRenderKey='';if(libraryState)renderLibrary(libraryState,false);message(error,true);}
  }finally{
    libraryViewPending=false;
    updateLibraryToolbar();
  }
}
async function saveLibraryOrder(ids,expected=libraryState?.orderEtag){
  if(libraryOrderPending||!online)return;
  const version=epoch;
  librarySortSaved=null;
  libraryOrderPending=true;updateLibraryToolbar();
  try{
    const saved=await libraryAction('正在保存排序…',async()=>{
      try{
        await libraryRequest({op:'reorder',ids,expected});
      }finally{
        if(online)await refreshLibrary();
      }
      return true;
    });
    if(saved&&version===epoch&&librarySortEditing)librarySortSaved={epoch:version,boot:libraryState?.boot};
  }finally{
    libraryOrderPending=false;
    libraryRenderKey='';if(libraryState)renderLibrary(libraryState,false);
    updateLibraryToolbar();
  }
}
let libraryDragLayer=null;
function clearLibraryDragLayer(){libraryDragLayer?.remove();libraryDragLayer=null;}
function showLibraryDragLayer(){
  const ghost=Sortable.ghost,list=$('libraryList');
  if(!ghost)return;
  clearLibraryDragLayer();
  // The body-level fallback needs the same layout selectors as the source.
  // Keep it outside the scrolling list so list overflow cannot clip the card.
  const layer=document.createElement('div');layer.className='library-list library-drag-layer';
  for(const key of ['layout','manual','selecting'])layer.dataset[key]=list.dataset[key];
  layer.setAttribute('aria-hidden','true');
  document.body.append(layer);layer.append(ghost);libraryDragLayer=layer;
}
function enableLibraryDrag(){
  if(librarySortable)return;
  librarySortable=Sortable.create($('libraryList'),{
    draggable:'.document-card[data-document-title]',handle:'.document-drag',animation:matchMedia('(prefers-reduced-motion: reduce)').matches?0:150,
    delay:150,delayOnTouchOnly:true,touchStartThreshold:4,ghostClass:'document-drag-ghost',chosenClass:'document-drag-chosen',
    forceFallback:true,fallbackOnBody:true,scroll:false,
    onStart:()=>{showLibraryDragLayer();libraryDragging=true;libraryDragAt=0;libraryDragFrame=requestAnimationFrame(scrollLibraryDrag);},
    onUnchoose:clearLibraryDragLayer,
    onEnd:event=>{
      clearLibraryDragLayer();
      libraryDragging=false;
      cancelAnimationFrame(libraryDragFrame);libraryDragFrame=0;libraryDragY=null;
      if(event.oldIndex===event.newIndex){
        libraryRenderKey='';renderLibrary(libraryState,false);
        if(libraryRefreshDeferred){libraryRefreshDeferred=false;libraryPollAt=0;void syncLibrary();}
        return;
      }
      const ids=[...$('libraryList').querySelectorAll('.document-card[data-document-title]')].map(card=>card.dataset.documentId);
      const visible=new Set(ids);let index=0;
      libraryRefreshDeferred=false;
      void saveLibraryOrder(orderedLibraryItems().map(item=>visible.has(item.id)?ids[index++]:item.id));
    }
  });
}
async function moveDocument(title,direction){
  if(!librarySortEditing||libraryView.sort!==6||$('librarySearch').value.trim()||libraryOrderPending||busy||libraryActionPending)return;
  const items=orderedLibraryItems(),from=items.findIndex(item=>item.title===title),to=from+direction;
  if(from<0||to<0||to>=items.length)return;
  [items[from],items[to]]=[items[to],items[from]];await saveLibraryOrder(items.map(item=>item.id));
}
let presenceVersion=-1,presenceReading=null,presenceAt=0,presenceInFlight=null;
async function syncPlaybackPresence(force=false,leaving=false){
  if(firmwareLocked())return;
  if(!online)return;
  const reading=!leaving&&!document.hidden&&nativeForeground&&pageName==='play',version=epoch;
  if(!force&&presenceVersion===version&&presenceReading===reading&&performance.now()-presenceAt<3000)return;
  // Reading liveness must not wait behind a slow SD operation or file upload.
  // Older firmware keeps the HTTP fallback until it advertises this capability.
  if(deviceSettings?.multiControl===undefined&&mode==='lan'&&lastState?.presenceHeartbeat&&liveSocket?.readyState===WebSocket.OPEN){
    try{
      liveSocket.send(JSON.stringify({heartbeat:true,reading}));
      presenceVersion=version;presenceReading=reading;presenceAt=performance.now();return;
    }catch{/* Fall back to an independent HTTP heartbeat. */}
  }
  if(presenceInFlight){await presenceInFlight;if(force)return syncPlaybackPresence(false,leaving);return;}
  let finishPresence;
  presenceInFlight=new Promise(resolve=>{finishPresence=resolve;});
  const request={op:'presence',client:clientId,reading,bluetooth:mode==='ble'};
  try{
    let result;
    if(mode==='lan')result=await performLibraryRequest(request);
    else{
      const pending=queue.then(()=>version===epoch&&online?libraryRequest(request):null);
      queue=pending.catch(()=>{});result=await pending;
    }
    if(result&&version===epoch){presenceVersion=version;presenceReading=reading;presenceAt=performance.now();if(typeof result.playbackAllowed==='boolean'){playbackAccess={epoch:version,allowed:result.playbackAllowed,at:performance.now()};buttons();}}
  }catch{/* A transient failure retries at the next heartbeat; the lease expires on disconnect. */}
  finally{
    presenceInFlight=null;finishPresence();
    if(!leaving&&online&&version===epoch&&reading!==(!document.hidden&&nativeForeground&&pageName==='play'))void syncPlaybackPresence(true);
  }
}
window.addEventListener('pagehide',()=>{
  if(!online)return;
  if(mode==='lan'){
    try{if(lastState?.presenceHeartbeat&&liveSocket?.readyState===WebSocket.OPEN)liveSocket.send(JSON.stringify({heartbeat:true,reading:false}));}catch{}
    fetch('/api/library',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({op:'presence',client:clientId,reading:false,bluetooth:false}),keepalive:true}).catch(()=>{});
  }else void syncPlaybackPresence(true,true);
});
window.addEventListener('pageshow',()=>void syncPlaybackPresence(true));
document.addEventListener('visibilitychange',()=>{
  if(!document.hidden){void syncPlaybackPresence(true);return;}
  if(online&&mode==='lan'){
    try{if(lastState?.presenceHeartbeat&&liveSocket?.readyState===WebSocket.OPEN)liveSocket.send(JSON.stringify({heartbeat:true,reading:false}));}catch{}
    fetch('/api/library',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({op:'presence',client:clientId,reading:false,bluetooth:false}),keepalive:true}).catch(()=>{});
  }
  else void syncPlaybackPresence(true,true);
});
document.addEventListener('teleprompter-app-state',event=>void syncPlaybackPresence(true,!event.detail.isActive));
function playbackDocumentPending(){
  const sameBoot=libraryState&&(!libraryState.boot||libraryState.boot===lastState?.boot);
  return !online||!lastState||libraryStateEpoch!==epoch||!sameBoot||!!playbackRestoreHint||
    (!libraryState.active&&(lastState.length>0||['starting','restarting','updating'].includes(lastState.lifecycle)));
}
function renderPlaybackDocument(){
  // Keep the current route while reconnecting and restoring this tab's selection.
  if(!online&&pageName==='play'&&libraryState?.active&&!playbackRestoreHint)
    playbackRestoreHint={title:libraryState.active,boot:lastState?.boot};
  const fresh=online&&lastState&&libraryState&&libraryStateEpoch===epoch&&libraryState.boot===lastState.boot;
  if(fresh&&playbackRestoreHint&&!['starting','restarting','updating','resetting'].includes(lastState.lifecycle)){
    if(libraryState.active)playbackRestoreHint=null;
    else if(lastState.length===0){
      playbackRestoreHint=null;
    }
  }
  const opening=pageTransition?.name==='play';
  const pending=playbackDocumentPending(),title=libraryState?.active;
  const label=online?'正在同步稿件…':'设备连接已断开，正在等待连接…';
  $('currentTitle').textContent=opening?(pageTransition.title||tr('正在载入提词…')):title||tr(pending?label:'尚未选择稿件');
  $('playEmpty').hidden=pending||!!title;
  $('playWorkspace').hidden=!title;
  $('playSync').hidden=!pending||!!title;
  $('playSync').querySelector('[role=status]').textContent=tr(label);
  $('playEdit').hidden=!title;setControlDisabled($('playEdit'),pending||firmwareLocked(),busy||playbackPending||libraryActionPending);
  if(opening){$('playWorkspace').hidden=false;$('playEmpty').hidden=true;$('playSync').hidden=true;setControlDisabled($('playEdit'),true);setPreviewStatus(tr('正在载入提词…'),'loading');}
  if(pending&&title&&pageName==='play'&&!libraryActionPending){
    if(!finishPlaybackSync)finishPlaybackSync=beginRegionLoading($('preview'),label);
    $('preview')._loadingIndicator.textContent=tr(label);
  }else{finishPlaybackSync?.();finishPlaybackSync=undefined;}
}
function beginRegionLoading(area,label){
  if(!area)return ()=>{};
  const indicator=document.createElement('div');
  indicator.className='action-loading';indicator.setAttribute('role','status');indicator.textContent=tr(label);
  const minimum=area.style.minHeight;
  const height=area.getBoundingClientRect().height;
  area.style.minHeight=Math.max(height,area.classList.contains('document-card')?0:120)+'px';
  area.setAttribute('aria-busy','true');area.classList.add('region-loading');
  area._loadingIndicator=indicator;area.append(indicator);
  if(area.id==='libraryList')requestAnimationFrame(fitLibraryFeedback);
  return ()=>{
    indicator.remove();delete area._loadingIndicator;
    area.removeAttribute('aria-busy');area.classList.remove('region-loading');area.style.minHeight=minimum;
  };
}
function syncLibraryFileActions(){
  for(const task of new Set(libraryFileActions.values())){
    if(task.epoch===epoch&&online&&!firmwareLocked()&&!task.cancelled)continue;
    task.cancelled=true;task.placeholder?.remove();
    for(const title of task.titles)if(libraryFileActions.get(title)===task)libraryFileActions.delete(title);
  }
  const list=$('libraryList'),activeCopies=new Set();
  for(const card of list.querySelectorAll('.document-card[data-document-title]')){
    const task=libraryFileActions.get(card.dataset.documentTitle);
    const overlayTask=task?.kind==='copy'?null:task;
    if(card._fileAction?.task!==overlayTask||card._fileAction?.layout!==libraryView.layout){card._fileAction?.finish();delete card._fileAction;}
    if(!task)continue;
    const label=task.started?task.label:task.waitLabel;
    if(task.kind==='copy'){
      // A duplicate is a new item; leave the source's content visible.
      if(task.resultTitle&&libraryState.items.some(item=>item.title===task.resultTitle)){task.placeholder?.remove();continue;}
      const signature=JSON.stringify([libraryView.layout,card.dataset.signature,uiLanguage]);
      if(!task.placeholder?.isConnected||task.placeholder.dataset.copySignature!==signature){
        task.placeholder?.remove();
        const placeholder=card.cloneNode(true);
        placeholder.classList.add('document-copy-placeholder');
        for(const key of ['documentTitle','documentId','signature','selected','playPosition'])delete placeholder.dataset[key];
        placeholder.dataset.copySource=card.dataset.documentTitle;placeholder.dataset.copySignature=signature;
        placeholder.inert=true;placeholder.setAttribute('aria-label',tr('副本')+' · '+card.dataset.documentTitle);
        for(const child of placeholder.querySelectorAll('[id]'))child.removeAttribute('id');
        // Hidden cloned content preserves the source's exact list/card geometry.
        card.after(placeholder);beginRegionLoading(placeholder,label);task.placeholder=placeholder;
      }else if(card.nextElementSibling!==task.placeholder)card.after(task.placeholder);
      task.placeholder._loadingIndicator.textContent=tr(label);activeCopies.add(task.placeholder);
      continue;
    }
    if(!card._fileAction)card._fileAction={task,layout:libraryView.layout,finish:beginRegionLoading(card,label)};
    card._loadingIndicator.textContent=tr(label);
  }
  for(const placeholder of list.querySelectorAll('.document-copy-placeholder'))if(!activeCopies.has(placeholder))placeholder.remove();
}
function queueLibraryFileAction(titles,label,waitLabel,operation,{kind='delete'}={}){
  if(!online||firmwareLocked()||librarySortEditing||titles.some(title=>libraryFileActions.has(title)))return Promise.resolve(false);
  const task={titles,epoch,boot:libraryState?.boot,storageGeneration:libraryState?.storageGeneration,label,waitLabel,kind,started:false,cancelled:false};
  const current=()=>!task.cancelled&&task.epoch===epoch&&online&&!firmwareLocked()&&task.boot===libraryState?.boot&&task.storageGeneration===libraryState?.storageGeneration;
  const ensureCurrent=()=>{if(!current())throw Error(tr('连接已变化，请重新检查稿件'));};
  const request=async values=>{ensureCurrent();const result=await libraryRequest(values,current);ensureCurrent();return result;};
  for(const title of titles)libraryFileActions.set(title,task);
  updateLibraryToolbar();
  // Share the device command queue without the page-wide interaction lock.
  const pending=queue.then(async()=>{
    try{
      ensureCurrent();task.started=true;++writeGeneration;syncLibraryFileActions();
      await operation(request,ensureCurrent,task);
      ensureCurrent();
      const state=await readStatus();if(current())render(state);
      return true;
    }catch(error){
      // Cancel the existing batch before the shared queue becomes available.
      // Swallowing a rejected promise alone would still run queued deletes.
      for(const waiting of new Set(libraryFileActions.values())){
        if(waiting===task||waiting.started||waiting.epoch!==task.epoch)continue;
        waiting.cancelled=true;waiting.placeholder?.remove();
        for(const title of waiting.titles)if(libraryFileActions.get(title)===waiting)libraryFileActions.delete(title);
      }
      syncLibraryFileActions();
      throw error;
    }
  });
  queue=pending.catch(()=>{});
  return pending.catch(error=>{if(task.epoch===epoch&&!task.cancelled)message(error,true);return false;}).finally(()=>{
    task.placeholder?.remove();
    for(const title of titles)if(libraryFileActions.get(title)===task)libraryFileActions.delete(title);
    buttons();
  });
}
async function libraryAction(label,operation,region=null,destination=null,targetTitle=''){
  if(libraryActionPending||busy||!online)return;
  const previousPage=pageName;
  libraryActionPending=true;buttons();
  const area=region?document.querySelector(region):null;
  const button=area?.tagName==='BUTTON';
  const finish=button?()=>{area.classList.remove('action-waiting');area.removeAttribute('aria-busy');}:beginRegionLoading(area,label);
  if(button){area.classList.add('action-waiting');area.setAttribute('aria-busy','true');}
  if(destination){
    pageTransition={name:destination,title:targetTitle};
    if(destination==='edit')$('editorTitle').textContent=targetTitle;
    if(destination==='play')resetDevicePreview(true);
    showPage(destination);
  }
  try{
    let result;
    await run(async()=>{result=await operation();return result;});
    return result;
  }catch{
    pageTransition=null;
    if(destination){showPage(previousPage);if(libraryState)renderLibrary(libraryState);}
  }finally{
    finish();
    pageTransition=null;
    if(destination==='edit')renderEditorHeading();
    libraryActionPending=false;buttons();
    renderPlaybackDocument();preview();
  }
}
let finishLibraryLoading;
function fitLibraryFeedback(){
  const list=$('libraryList');
  if(pageName!=='home'||(!list.classList.contains('region-loading')&&!list.querySelector('.library-empty')))return;
  const height=(window.visualViewport?.height||innerHeight)-Math.max(0,list.getBoundingClientRect().top)-24;
  list.style.setProperty('--library-feedback-height',Math.max(160,Math.floor(height))+'px');
}
window.addEventListener('resize',fitLibraryFeedback);
window.visualViewport?.addEventListener('resize',fitLibraryFeedback);
function libraryLoading(active){
  if(active){
    if(!finishLibraryLoading)finishLibraryLoading=beginRegionLoading($('libraryList'),'正在读取稿件库…');
  }else{
    finishLibraryLoading?.();finishLibraryLoading=undefined;
  }
}
const libraryErrors={edit_locked:'其他控制端正在编辑此稿件，当前仅可预览',edit_capacity:'同时编辑的稿件过多，请稍后重试',playback_locked:'其他控制端正在控制播放，当前仅可预览。',no_sd_card:'未检测到可用 SD 卡',invalid_title:'标题无效，请勿使用路径或特殊文件名字符',already_exists:'这个标题已存在',conflict:'其他控制端已修改，请重新读取后再保存',invalid_text:'正文需为 8192 字节以内的 UTF-8 文本',invalid_style:'样式文件无效，请在播放页恢复默认样式',missing_or_invalid_script:'稿件不存在、编码不正确或超过大小限制',storage_error:'存储读写失败，请保留当前草稿并重试',storage_changed:'稿件存储已切换，请刷新列表后重新操作；未保存的正文仍保留',storage_full:'存储空间不足，请先导出或删除不需要的稿件',storage_unavailable:'SD 卡和内置存储均不可用，请保留当前草稿',internal_storage_unavailable:'内置存储不可用，已有分区数据未被清空',invalid_request:'请求格式错误'};
function libraryRequest(request,shouldContinue=()=>true){
  const version=epoch;
  if(['get','select','save','create','delete','reorder','editLease'].includes(request.op))request={storageBackend:libraryState?.storageBackend,storageGeneration:libraryState?.storageGeneration,...request};
  const pending=bleLibraryQueue.then(async()=>{
    if(version!==epoch||apiPageLeaving)throw new DOMException('Connection changed','AbortError');
    if(!shouldContinue())throw Error('preview_superseded');
    try{return await performLibraryRequest(request,shouldContinue);}
    catch(error){
      const transient=['NetworkError','TimeoutError','InvalidStateError'].includes(error.name)||[408,429,502,503,504].includes(error.status)||/failed to fetch|load failed|GATT.*operation|GATT.*unknown/i.test(error.message);
      if(!['list','retry','get','preview','defaults'].includes(request.op)||!transient||version!==epoch||apiPageLeaving||!online||!shouldContinue())throw error;
      await new Promise(resolve=>setTimeout(resolve,200));
      if(version!==epoch||apiPageLeaving||!shouldContinue())throw new DOMException('Connection changed','AbortError');
      return performLibraryRequest(request,shouldContinue);
    }
  });
  bleLibraryQueue=pending.catch(()=>{});
  return pending;
}
async function performLibraryRequest(request,shouldContinue=()=>true){
  request={...request,client:clientId,timestamp:Math.floor(Date.now()/1000),timezoneOffset:new Date().getTimezoneOffset()};
  let response;
  if(mode==='lan')response=await(await api('/api/library',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(request)})).json();
  else{
    if(!bleLibrary)throw Error(tr('请升级设备固件后使用稿件库'));
    const bytes=encoder.encode(JSON.stringify(request));
    if(bytes.length>57344)throw Error(tr('请求过大'));
    await bleWrite(bleLibrary,packet(0x20,bytes.length,true));
    const payload=blePayloadSize();
    for(let offset=0;offset<bytes.length;offset+=payload){
      const part=bytes.slice(offset,offset+payload),frame=new Uint8Array(3+part.length);
      frame.set(packet(0x21,offset,true));frame.set(part,3);
      // Commit remains acknowledged; a lost fragment cannot publish a partial request.
      await bleWrite(bleLibrary,frame,!!bleLibrary.properties?.writeWithoutResponse);
    }
    await bleWrite(bleLibrary,packet(0x22));
    let total=0,offset=0,all;
    do{
      // Give newer controls priority over an obsolete preview download.
      if(!shouldContinue())throw Error('preview_superseded');
      await bleWrite(bleLibrary,Uint8Array.of(0x23,offset&255,(offset>>8)&255,(offset>>16)&255,(offset>>24)&255));
      const view=await bluetoothOperation(()=>bleLibrary.readValue());
      if(view.byteLength<8)throw Error(tr('蓝牙返回数据不完整'));
      const size=view.getUint32(0,true),start=view.getUint32(4,true);
      if(size>131072||start!==offset)throw Error(tr('蓝牙返回数据不完整'));
      if(!all){total=size;all=new Uint8Array(total);}else if(size!==total)throw Error(tr('蓝牙返回数据不完整'));
      const chunk=new Uint8Array(view.buffer,view.byteOffset+8,view.byteLength-8);
      if(!chunk.length&&offset<total||offset+chunk.length>total)throw Error(tr('蓝牙返回数据不完整'));
      all.set(chunk,offset);offset+=chunk.length;
    }while(offset<total);
    response=JSON.parse(decoder.decode(all));
  }
  if(response.storageBackend==='internal'&&response.sdMounted===false&&response.sdError&&response.sdError!=='ESP_OK'){
    libraryStorageFallback=true;
    libraryStorageFailure=tr('SD 卡已拔出或不可用，已自动切换到内部存储。');renderStorageNotice();
  }else if(response.storageBackend==='sd')libraryStorageFallback=false;
  if(!response.ok){
    console.warn('Device request rejected',request.op,response.error);
    if(response.error==='out_of_memory')throw Error(tr('设备内存暂不足，请稍后重试'));
    if(response.error==='firmware_busy')throw Error(tr('设备正在更新，请等待完成'));
    if(response.error==='order_conflict')throw Error(tr('排序已被其他控制端修改，请刷新后重试'));
    if(response.error==='invalid_order')throw Error(tr('稿件列表已变化，请刷新后重新排序'));
    if(['storage_changed','no_sd_card','storage_error','storage_unavailable','internal_storage_unavailable'].includes(response.error)){
      libraryStorageFailure=libraryStorageFallback?tr('SD 卡已拔出或不可用，已自动切换到内部存储。'):tr(libraryErrors[response.error]);renderStorageNotice();
      if(response.error==='storage_changed'){libraryPollAt=0;setTimeout(()=>{if(online)void refreshLibrary().catch(()=>{});},0);}
    }
    const wifiErrors={invalid_wifi_settings:'Wi-Fi 配置无效，请检查名称和密码',wifi_settings_failed:'Wi-Fi 设置失败，请重试',sd_format_failed:'SD 卡格式化失败，请重新插入后检查',sd_eject_failed:'SD 卡弹出失败，请重试',sd_busy:'SD 卡正在维护，请稍候',confirmation_required:'请先确认此操作会清空整张 SD 卡'};
    throw Error(tr(wifiErrors[response.error]||libraryErrors[response.error]||response.error));
  }
  return response;
}
function renderStorageNotice(){
  const internal=libraryState?.storageBackend==='internal';
  $('storageNotice').dataset.noticeType=libraryState?.storageBackend==='unavailable'?'error':'warning';
  const failed=libraryStorageFailure||(internal?tr('正在使用内置存储，稿件与 SD 卡分开保存；可在设置中切换。'):!libraryState?.mounted?tr('稿件存储不可用，请在设置中重新读取'):'');
  statusBanner($('storageNotice'),online&&libraryState&&failed?failed:null);
  $('storageStatus').textContent=failed;
  $('storageStatus').dataset.tooltip=libraryState?.storageError&&libraryState.storageError!=='ESP_OK'?libraryState.storageError:'';
  const identityIssues=libraryState?.items.filter(item=>item.identityIssue)||[];
  $('identityNoticeText').textContent=tr('有稿件无法确认原身份，已分配新 ID 并放在手动顺序末尾，请检查排序。')+' '+identityIssues.map(item=>item.title).join('、');
  statusBanner($('identityNotice'),online&&identityIssues.length?$('identityNoticeText').textContent:null);
}
function showPage(name){
  if(name!=='edit')releaseEditorLease();
  if(name!=='home')finishLibraryLayoutMotion();
  if(name!=='home')for(const ghost of document.querySelectorAll('.motion-ghost[data-motion-scope=library]')){motionJobs.get(ghost)?.cancel();ghost.remove();}
  if(name!=='play'&&typeof setPlaybackSheet==='function'){
    sheetDefaultKey=null;setPlaybackSheet(false,{focus:false,animate:false});
  }
  const changed=pageName!==name,previousPlaybackPage=pageName==='play';
  if(changed&&tabSessionReady&&!tabRestoring&&name!==tabSnapshot?.page)tabRestorePending=false;
  // Animate the destination only: outgoing clones expose stale manuscript data.
  pageName=name;
  if(changed&&(previousPlaybackPage||name==='play'))resetDevicePreview();
  void syncPlaybackPresence();
  document.body.dataset.page=name;
  scheduleViewportUpdate();
  for(const page of ['home','edit','play','settings'])$(page+'Page').hidden=name!==page;
  renderEditorConflict();
  if(changed&&typeof renderDevicePower==='function')renderDevicePower();
  renderPlaybackDocument();
  if(changed)motionEnter($(name+'Page'),'page',name==='home'?'back':'forward');
  preview();
  if(name==='home')requestAnimationFrame(fitLibraryFeedback);
  if(name==='settings'){requestAnimationFrame(layoutSettings);if(typeof refreshSdStatus==='function')void refreshSdStatus();}
  if(tabSessionReady)saveTabSession();
}
function layoutSettings(){
  const columns=matchMedia('(min-width:961px)').matches;
  for(const card of document.querySelectorAll('.settings-content>.wifi')){
    if(!columns){card.style.removeProperty('grid-row-end');continue;}
    if(card.hidden)continue;
    const rows=Math.ceil(card.getBoundingClientRect().height)+20;
    if(rows>20&&card.style.gridRowEnd!=='span '+rows)card.style.gridRowEnd='span '+rows;
  }
}
const settingsLayoutObserver=new ResizeObserver(layoutSettings);
for(const card of document.querySelectorAll('.settings-content>.wifi'))settingsLayoutObserver.observe(card);
window.addEventListener('resize',layoutSettings);
async function leaveEditor(name){
  if(documentDirty&&!await confirmAction(tr('放弃未保存修改？'),tr('未保存的内容只在当前页面内存中，离开后不会保存。'),{danger:true}))return;
  if(name!=='edit'){documentDirty=false;documentConflict=false;editingDocument=null;}
  showPage(name);
}
function markDocumentDirty(){if(editorReadOnly())return;documentDirty=true;$('draftState').textContent=tr('未保存');}
const actionIcons={
  settings:'<path d="m9.5 3-.7 2.2-2 .9-2.1-.5-2 3.4L4.2 11v2L2.7 15l2 3.4 2.1-.5 2 .9.7 2.2h5l.7-2.2 2-.9 2.1.5 2-3.4-1.5-2v-2l1.5-2-2-3.4-2.1.5-2-.9-.7-2.2Z"/><circle cx="12" cy="12" r="3.2"/>',
  refresh:'<path d="M20 5v5h-5M4 19v-5h5M19 9a8 8 0 0 0-14-3M5 15a8 8 0 0 0 14 3"/>',
  new:'<path d="M12 5v14M5 12h14"/>',
  disconnect:'<path d="M9 5H5v14h4M14 8l4 4-4 4M9 12h11"/>',
  prompt:'<rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8M12 17v4M8 8h8M8 12h5"/>',
  edit:'<path d="m14 5 5 5M4 20l5-1L21 7l-5-5L4 14Z"/>',
  delete:'<path d="M3 6h18M9 6V3h6v3M5 6l1 15h12l1-15M10 10v7M14 10v7"/>'
};
function setActionIcon(button,icon,label){
  button.type='button';button.classList.add('icon-button');
  button.setAttribute('aria-label',tr(label));button.dataset.actionLabel=label;button.dataset.tooltip=label;
  if(button.dataset.actionIcon!==icon){
    button.innerHTML='<svg viewBox="0 0 24 24" aria-hidden="true" focusable="false">'+actionIcons[icon]+'</svg>';
    button.dataset.actionIcon=icon;
  }
}
actionIcons.import='<path d="M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5"/>';
actionIcons.update='<path d="M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5"/>';
setActionIcon($('firmwareOnline'),'update','在线更新');
setActionIcon($('sdMount'),'refresh','重新读取');
actionIcons.screen='<rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8m-4-4v4"/>';
actionIcons.wake='<circle cx="12" cy="12" r="4"/><path d="M12 2v2m0 16v2M2 12h2m16 0h2M5 5l1.5 1.5m11 11L19 19M5 19l1.5-1.5m11-11L19 5"/>';
actionIcons.shutdown='<path d="M12 2v10M6 5a9 9 0 1 0 12 0"/>';
actionIcons.restart='<path d="M20 4v6h-6M20 10a8 8 0 1 0 0 5"/>';
actionIcons.search='<circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 5 5"/>';
actionIcons.reset='<path d="M20 4v6h-6M20 10a8 8 0 1 0 0 5" transform="translate(24 0) scale(-1 1)"/>';
actionIcons.back='<path d="m15 5-7 7 7 7"/>';
actionIcons.list='<path d="M9 5h12M9 12h12M9 19h12M3 5h1M3 12h1M3 19h1"/>';
actionIcons.grid='<rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/>';
actionIcons.select='<rect x="3" y="3" width="18" height="18" rx="4"/><path d="m7 12 3 3 7-7"/>';
actionIcons.sort='<path d="M4 5h16M4 12h11M4 19h6m8-5 3 3 3-3m-3-5v8" transform="translate(-1 0)"/>';
actionIcons.drag='<path d="M8 5h.01M16 5h.01M8 12h.01M16 12h.01M8 19h.01M16 19h.01" stroke-width="3" stroke-linecap="round"/>';
actionIcons.done='<path d="m5 12 4 4L19 6"/>';
actionIcons.close='<path d="m6 6 12 12M18 6 6 18"/>';
actionIcons.manual='<path d="M8 3v18m-3-3 3 3 3-3M16 21V3m-3 3 3-3 3 3"/>';
actionIcons.copy='<rect x="8" y="8" width="13" height="13" rx="2"/><path d="M16 8V5a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h3"/>';
setActionIcon($('libraryManualSort'),'manual','手动排序');
setActionIcon($('librarySortExit'),'close','退出排序');
for(const [id,icon,label] of [['libraryLayout','list','切换为列表'],['librarySort','sort','排序'],['libraryDeleteSelected','delete','删除所选稿件']])setActionIcon($(id),icon,label);
for(const [id,icon,label] of [['openSettings','settings','设置'],['libraryRefresh','refresh','刷新'],['libraryNew','new','新建稿件'],['reloadDocument','refresh','重新读取'],['import','import','导入文本文件'],['clear','delete','清空正文'],['deviceSleep','screen','息屏'],['deviceShutdown','shutdown','关机'],['deviceRestart','restart','重启']])setActionIcon($(id),icon,label);
setActionIcon($('librarySearchButton'),'search','搜索稿件');$('librarySearchButton').type='submit';
// Keep the left action buttons together and the search field at the right.
document.querySelector('.library-manage').append($('libraryLayout'),$('libraryNew'));
setActionIcon($('resetStyle'),'reset','恢复默认样式');
setActionIcon($('playEdit'),'edit','编辑稿件');
setActionIcon($('editorPrompt'),'prompt','提词');
for(const id of ['editBack','playBack','settingsBack'])setActionIcon($(id),'back','返回稿件库');
for(const id of ['apOpen','showInfo','firmwareAutomatic','firmwareOnlineAutomatic','rotation','mirror','loop','sightGuide','centerHighlight']){
  const input=$(id);input.setAttribute('role','switch');input.closest('label').classList.add('switch');
}
// Keep all reading adjustments together in one compact, responsive panel.
const playbackSettings=$('play').closest('section');playbackSettings.classList.add('playback-settings');
playbackSettings.id='playSettingsSheet';
function syncPlaybackSettingsInsets(){
  if(!playbackSettings.clientWidth)return;
  const style=getComputedStyle(playbackSettings);
  const gutter=Math.max(0,playbackSettings.offsetWidth-playbackSettings.clientWidth-parseFloat(style.borderLeftWidth)-parseFloat(style.borderRightWidth));
  const value=gutter+'px';
  if(playbackSettings.style.getPropertyValue('--settings-scrollbar-width')!==value)playbackSettings.style.setProperty('--settings-scrollbar-width',value);
}
new ResizeObserver(syncPlaybackSettingsInsets).observe(playbackSettings);
const sheetOpen=document.createElement('button');sheetOpen.id='playSettingsOpen';sheetOpen.type='button';sheetOpen.setAttribute('aria-controls','playSettingsSheet');sheetOpen.setAttribute('aria-expanded','false');
sheetOpen.innerHTML='<svg viewBox="0 0 24 24" aria-hidden="true">'+actionIcons.settings+'</svg><span>'+tr('播放设置')+'</span>';
$('playWorkspace').append(sheetOpen);
actionIcons.close='<path d="m6 6 12 12M18 6 6 18"/>';
const sheetClose=document.createElement('button');sheetClose.id='playSettingsClose';setActionIcon(sheetClose,'close','关闭播放设置');
actionIcons.expand='<path d="M8 3H3v5m13-5h5v5M3 16v5h5m13-5v5h-5"/>';
actionIcons.shrink='<path d="M3 8h5V3m8 0v5h5M8 21v-5H3m13 5v-5h5"/>';
const sheetExpand=document.createElement('button');sheetExpand.id='playSettingsExpand';sheetExpand.setAttribute('aria-controls','playSettingsSheet');sheetExpand.setAttribute('aria-pressed','false');setActionIcon(sheetExpand,'expand','全屏显示播放设置');
const sheetActions=document.createElement('div');sheetActions.className='card-head-actions';
const sheetHeading=playbackSettings.querySelector('.card-head');sheetHeading.append(sheetActions);sheetActions.append($('resetStyle'),sheetExpand,sheetClose);
const sheetBackdrop=document.createElement('div');sheetBackdrop.id='playSettingsBackdrop';document.body.append(sheetBackdrop);
let sheetPreviousFocus,sheetDefaultKey=null,sheetFullscreen=false,sheetMotion=null;
const sheetViewport=matchMedia('(max-width:650px), (pointer:coarse) and (max-height:600px) and (max-width:1100px)');
function sheetVisualState(){
  if(!sheetViewport.matches||pageName!=='play'||!playbackSettings.getClientRects().length)return null;
  const box=playbackSettings.getBoundingClientRect(),style=getComputedStyle(playbackSettings);
  return {left:box.left,top:box.top,width:box.width,height:box.height,opacity:style.opacity,borderRadius:style.borderRadius,boxShadow:style.boxShadow,scrollTop:playbackSettings.scrollTop};
}
function finishSheetMotion(){
  sheetMotion?.cancel();sheetMotion=null;
  delete playbackSettings.dataset.closing;
  playbackSettings.inert=false;
}
function animateSheet(from,to,{closing=false}={}){
  if(!motionEnabled()||!sheetViewport.matches||pageName!=='play'){finishSheetMotion();return;}
  const target=to||from;
  if(!target){finishSheetMotion();return;}
  const frame=box=>({left:box.left+'px',top:box.top+'px',width:box.width+'px',height:box.height+'px',right:'auto',bottom:'auto',maxHeight:'none',opacity:box.opacity,borderRadius:box.borderRadius,boxShadow:box.boxShadow,transform:'none'});
  const offset=document.body.dataset.playSettingsLayout==='side'?'translateX(36px)':'translateY(48px)';
  const first=from?frame(from):{...frame(target),opacity:0,transform:offset};
  const last=closing?{...frame(target),opacity:0,transform:offset}:frame(target);
  const job=motionAnimate(playbackSettings,[first,last],{duration:closing?200:260,fill:'both'});
  sheetMotion=job;
  if(from)playbackSettings.scrollTop=from.scrollTop;
  const settle=()=>{if(sheetMotion===job){sheetMotion=null;delete playbackSettings.dataset.closing;playbackSettings.inert=false;job?.cancel();}};
  if(job)job.finished.then(settle,settle);else settle();
  return job;
}
function syncPlaybackSheetDefault(portrait){
  // A transformed page temporarily contains fixed descendants. Open its
  // automatic drawer after the page settles, so it never jumps between origins.
  if(motionNavigation?.element===$('playPage'))return;
  const key=String(sheetViewport.matches);
  if(sheetDefaultKey===key)return;
  sheetDefaultKey=key;
  setPlaybackSheet(sheetViewport.matches&&!portrait,{focus:false});
}
function fitPlaybackSheet(){
  const open=playbackSettings.dataset.open==='true'&&sheetViewport.matches;
  document.body.dataset.playSettings=open?'open':'closed';
  document.body.dataset.playSettingsFullscreen=String(open&&sheetFullscreen&&pageName==='play');
  if(open){
    const height=window.visualViewport?.height||innerHeight;
    const top=$('playWorkspace').getBoundingClientRect().top;
    const landscape=innerWidth>height;
    const card=$('preview').closest('.preview-card'),style=getComputedStyle(card);
    const previewWidth=card.clientWidth-parseFloat(style.paddingLeft)-parseFloat(style.paddingRight)-2;
    const headingHeight=card.querySelector('.card-head').getBoundingClientRect().height;
    const previewHeight=previewWidth/(1280/800)+headingHeight+parseFloat(style.paddingTop)+parseFloat(style.paddingBottom)+10;
    const bottomPadding=parseFloat(getComputedStyle(document.querySelector('.shell')).paddingBottom)||12;
    const portrait=(lastState?.rotation??90)%180===0;
    const preferredHeight=!portrait&&!landscape?height-top-previewHeight-bottomPadding-10:Math.min(520,Math.round(height*.54));
    const sheetHeight=Math.max(150,Math.min(preferredHeight,height-top-170));
    document.body.style.setProperty('--play-sheet-height',sheetHeight+'px');
    document.body.style.setProperty('--play-sheet-top',Math.max(0,top)+'px');
    document.body.dataset.playSettingsLayout=landscape?'side':'bottom';
  }
  requestAnimationFrame(()=>{if(typeof fitDevicePreview==='function')fitDevicePreview();});
}
function setPlaybackSheetFullscreen(fullscreen){
  const from=sheetVisualState();finishSheetMotion();
  sheetFullscreen=Boolean(fullscreen&&sheetViewport.matches&&pageName==='play');
  setActionIcon(sheetExpand,sheetFullscreen?'shrink':'expand',sheetFullscreen?'恢复阅读预览':'全屏显示播放设置');
  sheetExpand.setAttribute('aria-pressed',String(sheetFullscreen));
  fitPlaybackSheet();
  const to=sheetVisualState();if(from&&to)animateSheet(from,to);
}
function setPlaybackSheet(open,{focus=true,animate=true}={}){
  const wasOpen=playbackSettings.dataset.open==='true',from=sheetVisualState();
  finishSheetMotion();
  const closing=!open&&animate&&from&&motionEnabled();
  // Keep the live panel rendered while its closing frame is installed. Hiding
  // it even briefly can reset scrolling before the exit animation begins.
  if(closing){playbackSettings.dataset.closing='true';playbackSettings.inert=true;}
  if(!open&&sheetFullscreen){
    sheetFullscreen=false;setActionIcon(sheetExpand,'expand','全屏显示播放设置');sheetExpand.setAttribute('aria-pressed','false');
  }
  if(open&&!wasOpen){sheetPreviousFocus=focus?document.activeElement:null;playbackSettings.scrollTop=0;}
  playbackSettings.dataset.open=String(open);sheetBackdrop.dataset.open=String(open);sheetOpen.setAttribute('aria-expanded',String(open));
  fitPlaybackSheet();
  if(open){
    playbackSettings.setAttribute('role','region');playbackSettings.setAttribute('aria-label',tr('播放设置'));
    if(focus)sheetClose.focus({preventScroll:true});
    if(animate&&!wasOpen)animateSheet(from,sheetVisualState());
  }else{
    playbackSettings.removeAttribute('role');
    const previousFocus=sheetPreviousFocus;sheetPreviousFocus=null;
    const restoreFocus=()=>{if(focus&&playbackSettings.dataset.open!=='true'&&previousFocus?.isConnected)previousFocus.focus({preventScroll:true});};
    const job=closing?animateSheet(from,null,{closing:true}):null;
    if(job)job.finished.then(restoreFocus,()=>{});else restoreFocus();
  }
}
sheetOpen.onclick=()=>setPlaybackSheet(true);sheetClose.onclick=()=>setPlaybackSheet(false);sheetBackdrop.onclick=()=>setPlaybackSheet(false);
sheetExpand.onclick=()=>setPlaybackSheetFullscreen(!sheetFullscreen);
document.addEventListener('keydown',event=>{
  if(playbackSettings.dataset.open!=='true')return;
  if(event.key==='Escape'&&!$('confirmDialog').open){event.preventDefault();if(sheetFullscreen)setPlaybackSheetFullscreen(false);else setPlaybackSheet(false);}
});
sheetViewport.addEventListener('change',()=>{if(!sheetViewport.matches)setPlaybackSheet(false);else fitPlaybackSheet();});
document.addEventListener('page-motion-end',event=>{
  if(pageName==='play'&&event.detail.element===$('playPage')&&typeof fitDevicePreview==='function')fitDevicePreview();
});
function resizePlaybackSheet(){finishSheetMotion();fitPlaybackSheet();}
window.addEventListener('resize',resizePlaybackSheet);window.visualViewport?.addEventListener('resize',resizePlaybackSheet);
const headerObserver=new ResizeObserver(()=>{document.documentElement.style.setProperty('--header-height',Math.ceil(document.querySelector('.top').getBoundingClientRect().height)+'px');if(typeof fitDevicePreview==='function')fitDevicePreview();});
headerObserver.observe(document.querySelector('.top'));
const settingsGrid=document.createElement('div');settingsGrid.className='playback-settings-grid';
playbackSettings.querySelector('.playback').after(settingsGrid);
for(const id of ['speed','font','letterSpacing','lineSpacing','countdown','brightness','marginLeft','textColor'])settingsGrid.append($(id).closest('.control'));
const readingGrid=playbackSettings.querySelector('.reading-grid');
const readingSwitches=playbackSettings.querySelector('.reading-switches');
const nextSwitch=document.createElement('label');nextSwitch.className='check switch';
nextSwitch.innerHTML='<input id="autoNext" type="checkbox" data-device disabled>自动播放下一个';readingSwitches.append(nextSwitch);
nextSwitch.querySelector('input').setAttribute('role','switch');
nextSwitch.querySelector('input').onchange=()=>control({autoNext:Number($('autoNext').checked)});
const boldSwitch=document.createElement('label');boldSwitch.className='check switch';
boldSwitch.innerHTML='<input id="bold" type="checkbox" role="switch" data-device disabled>加粗';readingSwitches.append(boldSwitch);
boldSwitch.querySelector('input').onchange=()=>control({bold:Number($('bold').checked)});
const orientationControl=$('rotation').closest('.control');
readingSwitches.prepend($('rotation').closest('label'),$('mirror').closest('label'));
orientationControl.remove();playbackSettings.append(readingSwitches);
const screenInfoControl=$('showInfo').closest('label');screenInfoControl.classList.add('screen-info-control');readingSwitches.append(screenInfoControl);
for(const label of readingSwitches.querySelectorAll('label.check')){
  const text=document.createElement('span');text.className='switch-text';
  for(const node of [...label.childNodes])if(node.nodeType===Node.TEXT_NODE)text.append(node);
  label.append(text);
}
// Separate playback behavior, typography and display options with shared groups.
function readingGroup(className,ids){
  const group=document.createElement('div');group.className=className+' playback-group';
  for(const id of ids)group.append($(id).closest('label.check')||$(id).closest('.control'));
  return group;
}
const behaviorGroup=readingGroup('reading-switches',['loop','autoNext']);behaviorGroup.id='playbackBehavior';
const spacingGroup=readingGroup('playback-settings-grid',['letterSpacing','lineSpacing','marginLeft','countdown']);spacingGroup.id='playbackSpacing';
const appearanceGroup=readingGroup('playback-settings-grid',['brightness','textColor']);appearanceGroup.id='playbackAppearance';
const remainingGroup=readingGroup('playback-settings-grid',['speed','font']);remainingGroup.id='playbackTypography';
settingsGrid.remove();
playbackSettings.querySelector('.playback').after($('pageReadingProgress'),behaviorGroup,spacingGroup,appearanceGroup,readingGrid,remainingGroup,readingSwitches);
readingGrid.classList.add('playback-group');readingSwitches.classList.add('playback-group');
for(const label of document.querySelectorAll('#deviceSettingsForm>label:not(.check)')){
  const title=document.createElement('span');title.className='setting-field-title';
  for(const node of [...label.childNodes])if(node.nodeType===Node.TEXT_NODE)title.append(node);
  label.prepend(title);
}
function documentDate(value){
  if(!Number.isFinite(value)||value<=0)return null;
  const date=new Date(value*1000);
  if(!Number.isFinite(date.getTime()))return null;
  return new Intl.DateTimeFormat(uiLanguage,{year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',hour12:false}).format(date);
}
function documentRelativeDate(value,now=Date.now()){
  if(!Number.isFinite(value)||value<=0)return '—';
  const date=new Date(value*1000);
  if(!Number.isFinite(date.getTime()))return '—';
  const seconds=Math.floor((now-date.getTime())/1000);
  const updated=text=>uiLanguage==='en'?'Updated '+text:text+'更新';
  if(seconds>=-5&&seconds<1)return updated(tr('刚刚'));
  if(seconds>=1&&seconds<60)return updated(seconds+tr('秒前'));
  if(seconds>=60&&seconds<3600)return updated(Math.floor(seconds/60)+tr('分钟前'));
  if(seconds>=3600&&seconds<86400)return updated(Math.floor(seconds/3600)+tr('小时前'));
  if(seconds>=86400&&seconds<=259200)return updated(Math.floor(seconds/86400)+tr('天前'));
  const options={month:'numeric',day:'numeric'};
  if(date.getFullYear()!==new Date(now).getFullYear())options.year='numeric';
  if(uiLanguage!=='en')return updated((options.year?date.getFullYear()+'年':'')+(date.getMonth()+1)+'月'+date.getDate()+'日');
  return updated(new Intl.DateTimeFormat(uiLanguage,options).format(date));
}
function refreshDocumentDates(now=Date.now()){
  if(pageName!=='home'||document.hidden)return;
  for(const detail of $('libraryList').querySelectorAll('dd[data-timestamp]')){
    const text=documentRelativeDate(Number(detail.dataset.timestamp),now);
    if(detail.textContent!==text)detail.textContent=text;
  }
}
// Update labels locally, preserving card nodes, selection and keyboard focus.
setInterval(refreshDocumentDates,1000);
document.addEventListener('visibilitychange',()=>refreshDocumentDates());
function documentSize(value){
  if(!Number.isFinite(value)||value<0)return tr('未记录');
  const units=['B','KB','MB','GB'];let unit=0;
  while(value>=1024&&unit<units.length-1){value/=1024;unit++;}
  return new Intl.NumberFormat(uiLanguage,{maximumFractionDigits:unit?1:0}).format(value)+' '+units[unit];
}
function libraryMotionPositions(cards){
  const positions=new Map(),top=document.querySelector('.library-header').getBoundingClientRect().bottom;
  for(const card of cards){
    const box=card.getBoundingClientRect();
    if(box.bottom>top&&box.top<innerHeight)positions.set(card,box);
  }
  return positions;
}
function animateLibraryPositions(positions){
  // Read every final position first; only transforms animate, never layout sizes.
  for(const card of positions.keys())motionJobs.get(card)?.cancel();
  const changes=[];
  for(const [card,before] of positions){
    if(!card.isConnected)continue;
    const after=card.getBoundingClientRect(),x=before.left-after.left,y=before.top-after.top;
    if(Math.abs(x)>.5||Math.abs(y)>.5)changes.push({card,x,y});
  }
  for(const {card,x,y} of changes)motionAnimate(card,[{transform:`translate(${x}px,${y}px)`},{transform:'none'}],{duration:220});
}
function renderLibrary(result,withView=true,appendOnly=false){
  if(libraryDragging&&!appendOnly)return;
  const preserveDrag=libraryDragging&&appendOnly;
  if(libraryState&&(result.active!==libraryState.active||result.storageGeneration!==libraryState.storageGeneration))resetDevicePreview();
  if(libraryState&&result.storageGeneration!==libraryState.storageGeneration){librarySelection.clear();librarySelectionMode=false;librarySortEditing=false;librarySearchResult=null;libraryRenderKey='';}
  libraryState=result;libraryStateEpoch=epoch;
  if(withView)applyLibraryView(result,result.settingsRevision);
  const titles=new Set(result.items.map(item=>item.title));
  for(const title of librarySelection)if(!titles.has(title))librarySelection.delete(title);
  renderPlaybackDocument();
  libraryStorageFailure=libraryStorageFallback?tr('SD 卡已拔出或不可用，已自动切换到内部存储。'):result.mounted&&result.storageError&&result.storageError!=='ESP_OK'?tr('SD 卡读写失败，原稿件已保留'):'';
  renderStorageNotice();
  updateLibraryToolbar();
  const query=$('librarySearch').value.trim().toLowerCase();
  const renderKey=JSON.stringify([result.items,result.active,result.mounted,query,librarySearchResult,uiLanguage,libraryView.layout,libraryView.sort]);
  if(renderKey===libraryRenderKey){renderDocumentStatus();return;}
  libraryRenderKey=renderKey;
  const list=$('libraryList');
  const oldCards=new Map([...list.querySelectorAll('.document-card[data-document-title]')].map(card=>[card.dataset.documentTitle,card]));
  const previous=list._renderedLibrary;
  const membershipChanged=previous&&(previous.titles.size!==titles.size||[...titles].some(title=>!previous.titles.has(title)));
  const animateChange=membershipChanged&&previous.epoch===epoch&&previous.boot===result.boot&&previous.mounted&&result.mounted&&previous.layout===libraryView.layout&&previous.query===query&&pageName==='home'&&!document.hidden&&!preserveDrag&&!librarySortEditing&&!list.classList.contains('region-loading')&&motionEnabled();
  list._renderedLibrary={epoch,boot:result.boot,mounted:result.mounted,layout:libraryView.layout,query,titles};
  const positions=animateChange?libraryMotionPositions(oldCards.values()):new Map();
  // Only confirmed removals leave the full library; filtering is not deletion.
  if(animateChange)for(const [title,card] of oldCards)if(!titles.has(title)&&positions.has(card))motionExit(card,'library');
  const keep=new Set();list.querySelector('.library-empty')?.remove();
  let previousCard=null;
  const placeCard=card=>{
    if(preserveDrag){
      list.insertBefore(card,list._loadingIndicator?.parentNode===list?list._loadingIndicator:null);
    }else{
      const next=previousCard?previousCard.nextElementSibling:list.firstElementChild;
      if(next!==card)list.insertBefore(card,next);
      previousCard=card;
    }
  };
  const items=visibleLibraryItems();
  if(!items.length){const empty=document.createElement('p');empty.className='library-empty';empty.setAttribute('role','status');empty.textContent=tr(!result.mounted?'稿件存储不可用，请在设置中重新读取。':query?'没有找到匹配的稿件':result.storageBackend==='internal'?'内置稿件库为空，可新建或导入文稿。':'暂无稿件，可新建或将 txt 文件放入 SD 卡的 teleprompter 文件夹。');list.append(empty);requestAnimationFrame(fitLibraryFeedback);}
  for(const item of items){
    // Selection and order change existing nodes; only visible content needs rebuilding.
    const signature=JSON.stringify([item.id,item.title,item.excerpt,item.bytes,item.characters,item.createdAt,item.updatedAt,uiLanguage]);
    const existing=oldCards.get(item.title);keep.add(item.title);
    // Pagination may append while dragging, but must never move/replace a held card.
    if(preserveDrag&&existing)continue;
    if(existing?.dataset.signature===signature){placeCard(existing);continue;}
    existing?.remove();
    const card=document.createElement('article');card.className='document-card';card.dataset.documentTitle=item.title;card.dataset.documentId=item.id||'';
    card.dataset.signature=signature;
    const drag=document.createElement('button');drag.className='document-drag';setActionIcon(drag,'drag','拖动排序');
    drag.dataset.tooltip=tr($('librarySearch').value.trim()?'清空搜索后拖动排序':'拖动排序，也可用上下方向键移动');
    drag.onkeydown=event=>{if(event.key==='ArrowUp'||event.key==='ArrowDown'){event.preventDefault();void moveDocument(item.title,event.key==='ArrowUp'?-1:1);}};

    const select=document.createElement('label');select.className='document-select';
    const checkbox=document.createElement('input');checkbox.type='checkbox';checkbox.checked=librarySelection.has(item.title);
    checkbox.setAttribute('aria-label',tr('选择稿件')+' '+item.title);
    checkbox.onchange=()=>{if(checkbox.checked)librarySelection.add(item.title);else librarySelection.delete(item.title);updateLibraryToolbar();};
    select.append(checkbox);
    const status=document.createElement('span');status.className='document-play-status';status.hidden=true;
    const h=document.createElement('h3'),titleText=document.createElement('span');titleText.className='document-title-text';titleText.textContent=item.title;h.append(drag,select,titleText,status);card.append(h);
    const excerpt=document.createElement('p');excerpt.className='document-excerpt';excerpt.textContent=item.excerpt?.replace(/\s+/g,' ').trim()||tr('暂无正文');card.append(excerpt);
    const meta=document.createElement('dl');meta.className='document-metadata';
    for(const [label,value,kind] of [['字数',Number.isFinite(item.characters)?new Intl.NumberFormat(uiLanguage).format(item.characters)+tr(' 字'):tr('未记录'),'size'],['文件大小',documentSize(item.bytes),'size'],['更新时间',documentDate(item.updatedAt),'date']]){
      if(label==='更新时间'&&(value===null||(documentDate(item.createdAt)!==null&&item.updatedAt===item.createdAt)))continue;
      const row=document.createElement('div'),term=document.createElement('dt'),detail=document.createElement('dd');
      row.className='document-meta-'+kind;
      term.textContent=tr(label);detail.textContent=value??'—';
      if(kind==='date'){
        row.dataset.dateKind='updated';
        row.dataset.tooltip=tr(label)+' '+value;
        term.className='sr-only';
        const timestamp=item.updatedAt;
        if(value!==null){detail.dataset.timestamp=timestamp;detail.textContent=documentRelativeDate(timestamp);}
      }
      row.append(term,detail);meta.append(row);
    }card.append(meta);
    const actions=document.createElement('div');actions.className='document-actions';
    for(const [text,icon,handler] of [['提词','prompt',()=>openPlayback(item.title)],['编辑','edit',()=>openEditor(item.title)],['复制为副本','copy',()=>duplicateDocument(item.title)],['删除','delete',()=>deleteDocument(item.title)]]){
      const button=document.createElement('button');button.dataset.device='';setControlDisabled(button,!online,busy||libraryActionPending);if(text==='提词')button.className='primary';if(text==='删除')button.className='ghost danger';setActionIcon(button,icon,text);button.onclick=handler;actions.append(button);
    }
    card.append(actions);placeCard(card);
    if(!existing&&pageName==='home'&&!preserveDrag)motionAnimate(card,[{opacity:0,transform:'translateY(8px) scale(.985)'},{opacity:1,transform:'none'}],{duration:200,delay:Math.min(list.children.length-1,5)*20,fill:'backwards'});
  }
  if(!preserveDrag)for(const [title,card] of oldCards)if(!keep.has(title))card.remove();
  if(list._loadingIndicator)list.append(list._loadingIndicator);
  enableLibraryDrag();
  renderDocumentStatus();
  updateLibraryMore();
  if(positions.size)animateLibraryPositions(positions);
}
function renderDocumentStatus(){
  updateNextDocument();
  updateLibraryToolbar();
  const ordered=orderedLibraryItems(),activeIndex=ordered.findIndex(item=>item.title===libraryState?.active);
  const previous=activeIndex>0?ordered[activeIndex-1].title:null;
  const next=activeIndex>=0?ordered[activeIndex+1]?.title:null;
  for(const card of $('libraryList').querySelectorAll('.document-card[data-document-title]')){
    card.dataset.unavailable=String(!online);
    card.setAttribute('aria-disabled',String(!online));
    const badge=card.querySelector('.document-play-status');
    const title=card.dataset.documentTitle,active=libraryState?.active===title;
    const adjacent=title===previous?'上一个':title===next?'下一个':null;
    badge.hidden=!active&&!adjacent;
    badge.dataset.position=active?'current':title===previous?'previous':title===next?'next':'';
    card.dataset.playPosition=badge.dataset.position;
    const playing=active&&online&&lastState?.playing&&!deviceSettings?.asleep;
    badge.dataset.playing=String(playing);
    badge.textContent=tr(active?(!online?'已载入':playing?'播放中':'使用中'):adjacent||'');
    badge.dataset.tooltip=tr(active?(playing?'此稿件正在设备上播放':'设备当前载入的稿件'):title===previous?'按当前排序，将播放的上一篇稿件':'按当前排序，将播放的下一篇稿件');
  }
}
function updateNextDocument(){
  const items=orderedLibraryItems(),index=items.findIndex(item=>item.title===libraryState?.active);
  const pending=busy||playbackPending||libraryActionPending;
  setControlDisabled($('previousDocument'),!online||firmwareLocked()||index<=0,pending);
  setControlDisabled($('nextDocument'),!online||firmwareLocked()||index<0||index>=items.length-1,pending);
  const noNext=index<0||index>=items.length-1;
  setControlDisabled($('autoNext'),!online||firmwareLocked()||noNext,pending);
  $('autoNext').closest('label').dataset.tooltip=tr(noNext?'当前没有下一篇稿件，无法开启自动播放下一篇。':'当前稿件播放结束后，自动播放排序中的下一篇稿件。');
}
async function playAdjacentDocument(direction){
  if(!online||busy||playbackPending||libraryActionPending)return;
  return libraryAction('正在载入提词…',async()=>{
    // Resolve adjacent entries from the full library, not search results.
    await refreshLibrary();
    const items=orderedLibraryItems(),index=items.findIndex(item=>item.title===libraryState.active);
    const target=index+direction;
    if(index<0||target<0||target>=items.length)return;
    const title=items[target].title;
    pageTransition={name:'play',title};resetDevicePreview(true);renderPlaybackDocument();
    const selected=await libraryRequest({op:'select',title});
    activeDocument=null;
    renderLibrary({...libraryState,active:title,revision:selected.revision});libraryPollAt=performance.now();
    showPage('play');
    // Manual navigation selects a stopped manuscript. Only the device's
    // automatic-next path may start the next manuscript without a Play click.
  },'#preview');
}
async function listAll(op='list',query='',current=()=>true,compact=false){
  const values={...(query?{query}:{}),...(compact?{index:true}:{})};
  let result=await libraryRequest({op,...values},current),items=[...result.items];
  while(result.next>=0){if(!current())throw Error('preview_superseded');const next=await libraryRequest({op:'list',offset:result.next,...values},current);if(next.orderEtag!==result.orderEtag||next.revision!==result.revision||next.boot!==result.boot)throw Error(tr('稿件列表已变化，请刷新后重新排序'));items.push(...next.items);result={...next,items};}
  result.items=items;return result;
}
async function prepareLibraryPage(guard=()=>true){
  if(!libraryState)return;
  const generation=++libraryPageGeneration,version=epoch;
  libraryPagePending=false;libraryPageError='';
  const current=()=>generation===libraryPageGeneration&&version===epoch&&guard();
  await fillLibraryDetails(matchingLibraryItems().slice(0,libraryPageLimit),current);
  if(current()){libraryRenderKey='';renderLibrary(libraryState,false);}
}
async function refreshLibrary(op='list'){
  if(libraryDragging){libraryRefreshDeferred=true;return;}
  const version=epoch;
  ++librarySearchGeneration;clearTimeout(librarySearchTimer);finishSearchLoading?.();finishSearchLoading=null;
  const generation=++libraryPageGeneration,current=()=>version===epoch&&generation===libraryPageGeneration;
  libraryPagePending=false;libraryPageError='';
  const showLoading=!libraryState||op==='retry';
  if(showLoading)libraryLoading(true);
  try{
    let result=await listAll(op,'',current,true);
    if(!current())return;
    if(libraryDragging){libraryRefreshDeferred=true;return;}
    const changed=!libraryState||libraryState.boot!==result.boot||libraryState.revision!==result.revision||libraryStateEpoch!==epoch||op==='retry';
    if(changed){
      if(!libraryState||libraryState.boot!==result.boot||libraryStateEpoch!==epoch||op==='retry')libraryPageLimit=LIBRARY_PAGE_SIZE;
      libraryDetails.clear();librarySearchResult=null;
    }
    // renderPlaybackDocument settles reconnects from this fresh library and
    // status pair. Never reselect an old manuscript over another controller.
    if(current()){
      if(libraryState&&result.active!==libraryState.active)resetDevicePreview();
      libraryState=result;libraryStateEpoch=epoch;
      applyLibraryView(result,result.settingsRevision,false);
      await prepareLibraryPage();
      if($('librarySearch').value.trim())void searchLibrary();
    }
  }
  finally{if(showLoading)libraryLoading(false);requestAnimationFrame(checkLibraryScroll);}
}
function renderEditorHeading(){
  const saved=!!editingDocument?.etag;
  $('editorTitle').textContent=pageTransition?.name==='edit'?pageTransition.title:saved?editingDocument.title:tr('新建稿件');
  $('documentTitleField').hidden=saved;
  $('documentTitle').disabled=saved;
  $('reloadDocument').hidden=!saved;
  renderEditorConflict();
}
function renderEditorConflict(){
  statusBanner($('editorConflictNotice'),pageName==='edit'&&documentConflict?'conflict':null);
  setControlDisabled($('editorConflictReload'),!online||firmwareLocked()||devicePowerBlocked(),busy);
  renderEditorLease();
}
async function openEditor(title){
  if(documentDirty&&!await confirmAction(tr('放弃未保存修改？'),tr('未保存的内容只在当前页面内存中，离开后不会保存。'),{danger:true}))return;
  releaseEditorLease();
  if(!title){
    editingDocument={title:'',text:'',etag:null,hasStyle:false,styleValid:true,storageBackend:libraryState?.storageBackend,storageGeneration:libraryState?.storageGeneration};
    documentDirty=false;documentConflict=false;$('documentTitle').value='';renderEditorHeading();
    $('script').value='';lastSent='';count();$('draftState').textContent=tr('未保存');showPage('edit');return;
  }
  return libraryAction('正在读取稿件…',async()=>{
    const doc=await libraryRequest({op:'get',title});
    editingDocument=doc;documentDirty=false;documentConflict=false;
    startEditorLease(doc);
    $('documentTitle').value=doc.title;renderEditorHeading();
    $('script').value=doc.text;lastSent=doc.text;count();
    $('draftState').textContent=tr(title?(doc.storageBackend==='internal'?'已保存到内置存储':'已保存到 SD 卡'):'未保存');showPage('edit');
    if(!doc.styleValid)message(tr('样式文件无效，请在播放页恢复默认样式'),true);
  },'#editPage .editor','edit',title).finally(()=>void checkEditorLease());
}
async function duplicateDocument(title){
  return queueLibraryFileAction([title],'正在复制稿件…','等待复制…',async(request,ensureCurrent,task)=>{
    const original=await request({op:'get',title});
    await refreshLibrary();
    ensureCurrent();
    const names=new Set(libraryState.items.map(item=>item.title));
    let base=title,name,created=false;
    while(encoder.encode(base+' - '+tr('副本')+' (9999)').length>96)base=Array.from(base).slice(0,-1).join('');
    for(let n=1;n<10000;n++){
      name=base+' - '+tr('副本')+(n===1?'':' ('+n+')');
      if(names.has(name))continue;
      try{await request({op:'create',title:name,text:original.text,...(original.hasStyle&&original.styleValid?{style:original.style}:{})});created=true;break;}
      catch(error){if(error.message===tr(libraryErrors.already_exists)){names.add(name);continue;}throw error;}
    }
    if(!created)throw Error(tr('这个标题已存在'));
    task.resultTitle=name;
    await refreshLibrary();ensureCurrent();message(tr('已复制为副本')+' · '+name);
  },{kind:'copy'});
}
async function openPlayback(title){
  if(documentDirty&&!await confirmAction(tr('放弃未保存修改？'),tr('未保存的内容只在当前页面内存中，离开后不会保存。'),{danger:true}))return;
  return libraryAction('正在载入提词…',async()=>{
    if(deviceSettings?.multiControl!==undefined){
      const access=await libraryRequest({op:'presence',client:clientId,reading:!document.hidden&&nativeForeground,bluetooth:mode==='ble'});
      playbackAccess={epoch,allowed:!!access.playbackAllowed,at:performance.now()};
      if(!access.playbackAllowed){documentDirty=false;editingDocument=null;await refreshLibrary();showPage('play');return;}
    }
    const selected=await libraryRequest({op:'select',title});
    activeDocument=null;
    documentDirty=false;editingDocument=null;
    // Selection changes the active title and revision, not the library metadata.
    renderLibrary({...libraryState,active:title,revision:selected.revision});
    libraryPollAt=performance.now();showPage('play');
    return await applyControl({playing:0});
  },'#preview','play',title);
}
async function promptFromEditor(){
  if(!editingDocument||libraryActionPending||busy||!online)return;
  if(documentDirty||!editingDocument.etag){
    if(!await saveDocument({chooseNext:false}))return;
  }
  if(!documentDirty&&editingDocument?.etag)await openPlayback(editingDocument.title);
}
async function saveDocument({chooseNext=true}={}){
  if(!editingDocument||libraryActionPending||busy||!online||editorReadOnly())return;
  const title=editingDocument.etag?editingDocument.title:$('documentTitle').value,text=$('script').value;
  if(!title||title.trim()!==title||encoder.encode(title).length>96||/[\x00-\x1f/\\:*?"<>|]/.test(title)||title.startsWith('.')||title.endsWith('.')){message(tr('标题无效，请勿使用路径或特殊文件名字符'),true);return;}
  if(encoder.encode(text).length>8192){message(tr('正文需为 8192 字节以内的 UTF-8 文本'),true);return;}
  if(documentConflict){renderEditorConflict();return;}
  const draft={title,text};
  const snapshot=JSON.stringify(draft);
  const created=!editingDocument.etag,version=epoch;
  const saved=await libraryAction('正在保存稿件…',async()=>{
    const previous=$('draftState').textContent;$('draftState').textContent=tr('正在保存稿件…');
    let result;
    try{result=await libraryRequest({op:editingDocument.etag?'save':'create',...draft,expected:editingDocument.etag,...(editingDocument.etag?{editClient:editorLease?.client}:{}),storageBackend:editingDocument.storageBackend,storageGeneration:editingDocument.storageGeneration});}
    catch(error){$('draftState').textContent=previous;throw error;}
    editingDocument={...editingDocument,title,text,etag:result.etag};
    releaseEditorLease();
    renderEditorHeading();
    if(snapshot===JSON.stringify({title:$('documentTitle').value,text:$('script').value}))documentDirty=false;
    lastSent=text;$('draftState').textContent=tr(result.storageBackend==='internal'?'已保存到内置存储':'已保存到 SD 卡');count();
    startEditorLease(editingDocument);
    await refreshLibrary();
    if(!created||!chooseNext)message(tr(result.storageBackend==='internal'?'已保存到内置存储':'已保存到 SD 卡'));
    return editingDocument;
  },'#send');
  void checkEditorLease();
  if(saved&&created&&chooseNext&&version===epoch&&pageName==='edit'&&!documentDirty){
    const dialog=$('savedDocumentDialog');dialog.returnValue='';
    const choice=await new Promise(resolve=>{dialog.addEventListener('close',()=>resolve(dialog.returnValue),{once:true});dialog.showModal();motionEnter(dialog,'dialog');});
    if(version===epoch&&pageName==='edit'&&editingDocument===saved&&!documentDirty){
      if(choice==='prompt')await openPlayback(saved.title);
      else if(choice==='home')await leaveEditor('home');
    }
  }
  return saved;
}
async function deleteDocument(title){
  if(libraryFileActions.has(title)||!online||firmwareLocked())return;
  const version=epoch;
  if(!await confirmAction(tr('删除稿件？'),title+'\n'+tr('同时删除同名样式文件。此操作不可撤销。'),{danger:true}))return;
  if(version!==epoch)return;
  return queueLibraryFileAction([title],'正在删除稿件…','等待删除…',async(request,ensureCurrent)=>{
    const doc=await request({op:'get',title});
    await request({op:'delete',title,expected:doc.etag});
    if(activeDocument?.title===title)activeDocument=null;
    librarySelection.delete(title);
    await refreshLibrary();ensureCurrent();message(tr('稿件已删除'));
  });
}
let batchConfirmPending=false;
async function deleteSelectedDocuments(){
  if(batchConfirmPending||!librarySelection.size||!online||busy||libraryActionPending)return;
  const titles=[...librarySelection].filter(title=>!libraryFileActions.has(title)),version=epoch;
  if(!titles.length)return;
  batchConfirmPending=true;
  try{
    const description=tr('将删除所选稿件及同名样式文件，无法撤销。');
    if(!await confirmAction(tr('删除所选稿件')+' ('+titles.length+')',description,{danger:true,items:titles,delay:3})||version!==epoch)return;
    return queueLibraryFileAction(titles,'正在删除稿件…','等待删除…',async(request,ensureCurrent)=>{
      let deleted=0;
      try{
        // Capture every version before the first deletion; concurrent edits still conflict.
        const snapshots=[];
        for(const title of titles)snapshots.push(await request({op:'get',title}));
        for(let i=0;i<titles.length;i++){
          if(version!==epoch||!online)throw Error(tr('连接已变化，请重新检查稿件'));
          await request({op:'delete',title:titles[i],expected:snapshots[i].etag});
          if(activeDocument?.title===titles[i])activeDocument=null;
          librarySelection.delete(titles[i]);deleted++;
        }
      }catch(error){
        if(deleted)throw Error(tr('已删除')+' '+deleted+' / '+titles.length+' '+tr('篇稿件')+'；'+userError(error));
        throw error;
      }finally{
        if(version===epoch&&online)await refreshLibrary();
        updateLibraryToolbar();
      }
      ensureCurrent();librarySelectionMode=librarySelection.size>0;libraryRenderKey='';renderLibrary(libraryState);
      message(tr('已删除')+' '+deleted+' '+tr('篇稿件'));
    });
  }finally{batchConfirmPending=false;}
}
async function syncLibrary(){
  if(libraryDragging||libraryOrderPending){libraryRefreshDeferred=true;return;}
  const changed=lastState?.libraryRevision!==undefined&&libraryState?.revision!==lastState.libraryRevision;
  const interval=playbackDocumentPending()||changed?500:lastState?.libraryRevision!==undefined?60000:10000;
  if(libraryBusy||performance.now()-libraryPollAt<interval)return;
  const version=epoch;
  libraryBusy=true;libraryPollAt=performance.now();
  try{
    const pending=queue.then(async()=>{
      await refreshLibrary();
      if(version!==epoch||!libraryState)return;
      // Preview has its own device snapshot; downloading the active text again is redundant.
      if(!libraryState.active)activeDocument=null;
      preview();
      if(editingDocument?.etag&&!editorReadOnly()){
        const remote=await libraryRequest({op:'get',title:editingDocument.title});
        if(remote.etag!==editingDocument.etag){
          if(documentDirty&&remote.text===editingDocument.text){editingDocument={...remote};}
          else if(documentDirty&&!documentConflict){documentConflict=true;$('draftState').textContent=tr('其他端已修改，请重新读取');renderEditorConflict();}
          else if(!documentDirty){editingDocument=remote;$('script').value=remote.text;lastSent=remote.text;count();}
        }
      }
    });queue=pending.catch(()=>{});await pending;
  }catch(e){if(version===epoch&&e.message!=='preview_superseded')message(e,true);}finally{libraryBusy=false;requestAnimationFrame(checkLibraryScroll);}
}
$('editBack').onclick=$('playBack').onclick=$('settingsBack').onclick=()=>leaveEditor('home');
$('openSettings').onclick=()=>{if(!$('openSettings').disabled)return leaveEditor('settings');};
$('libraryRefresh').onclick=()=>{if(!$('libraryRefresh').disabled){libraryLoading(true);return libraryAction('正在刷新稿件库…',()=>refreshLibrary('retry'));}};
$('libraryNew').onclick=()=>openEditor(null);
let editPlaybackConfirmPending=false;
$('playEdit').onclick=async()=>{
  if(editPlaybackConfirmPending||libraryActionPending||busy||!online||!libraryState?.active)return;
  const title=libraryState.active,version=epoch;editPlaybackConfirmPending=true;
  try{
    if(!await confirmAction(tr('离开提词并编辑？'),tr('将暂停当前播放并进入稿件编辑，屏幕会按其他控制端的使用状态决定是否休眠。'))||version!==epoch||title!==libraryState?.active)return;
    if(lastState?.playing||countdownDeadline)await run(()=>applyControl({playing:0}));
    await openEditor(title);
  }catch{/* Operation errors are presented by run(). */}
  finally{editPlaybackConfirmPending=false;}
};
$('nextDocument').onclick=()=>playAdjacentDocument(1);
$('previousDocument').onclick=()=>playAdjacentDocument(-1);
$('chooseDocument').onclick=()=>showPage('home');
$('librarySearch').oninput=()=>{
  if($('librarySearch').disabled||!libraryState)return;
  clearTimeout(librarySearchTimer);++librarySearchGeneration;++libraryPageGeneration;libraryPagePending=false;
  if(!$('librarySearch').value.trim())void searchLibrary();
  else{
    // The debounce is already part of searching: don't expose a local empty
    // result before the device has searched the full manuscript contents.
    finishSearchLoading ||= beginRegionLoading($('libraryList'),'正在搜索稿件…');
    librarySearchResult=null;renderLibrary(libraryState);librarySearchTimer=setTimeout(searchLibrary,300);
  }
};
$('librarySearchForm').onsubmit=event=>{event.preventDefault();if(!$('librarySearchButton').disabled&&libraryState)void searchLibrary();};
$('librarySelectAll').onchange=()=>{
  librarySelectionMode=$('librarySelectAll').checked;
  if(librarySelectionMode)librarySortEditing=false;
  if(librarySelectionMode){for(const item of matchingLibraryItems())if(!libraryFileActions.has(item.title))librarySelection.add(item.title);}
  else librarySelection.clear();
  libraryRenderKey='';renderLibrary(libraryState);
};
$('libraryDeleteSelected').onclick=deleteSelectedDocuments;
$('libraryLayout').onclick=()=>saveLibraryView({libraryLayout:libraryView.layout?0:1});
$('libraryManualSort').onclick=async()=>{
  if($('libraryManualSort').disabled)return;
  closeLibrarySort();
  if(librarySortEditing)return;
  if($('librarySearch').value.trim()){
    $('librarySearch').value='';
    try{await searchLibrary();}catch(error){message(error,true);return;}
  }
  if(libraryView.sort!==6)await saveLibraryView({librarySort:6});
  if(libraryView.sort===6&&online){librarySelectionMode=false;librarySelection.clear();librarySortSaved=null;librarySortEditing=true;libraryRenderKey='';renderLibrary(libraryState);updateLibraryToolbar();$('librarySortExit').focus({preventScroll:true});}
};
$('librarySortExit').onclick=()=>{
  if($('librarySortExit').disabled||!librarySortEditing)return;
  const saved=librarySortSaved?.epoch===epoch&&librarySortSaved?.boot===libraryState?.boot;
  librarySortEditing=false;librarySortSaved=null;updateLibraryToolbar();
  $('libraryManualSort').focus({preventScroll:true});
  if(saved)message(tr('排序已保存'));
};
$('librarySort').onclick=()=>{
  if($('librarySort').disabled)return;
  const opened=$('librarySortMenu').hidden;$('librarySortMenu').hidden=!opened;
  $('librarySort').setAttribute('aria-expanded',String(opened));
  if(opened)($('librarySortMenu').querySelector('[aria-checked=true]')||$('librarySortMenu').querySelector('button'))?.focus();
};
for(const option of $('librarySortMenu').querySelectorAll('button'))option.onclick=async()=>{
  if(option.disabled||$('librarySort').disabled)return;
  const field=option.dataset.sortField!==undefined?Number(option.dataset.sortField):Math.floor(libraryView.sort/2);
  const ascending=option.dataset.sortDirection!==undefined?option.dataset.sortDirection==='asc':libraryView.sort===6?field===0:librarySortAscending();
  const sort=librarySortValue(field,ascending);
  if(sort!==libraryView.sort)await saveLibraryView({librarySort:sort});
  librarySortEditing=false;
  closeLibrarySort();updateLibraryToolbar();
};
document.addEventListener('click',event=>{if(!event.target.closest('.library-sort-wrap'))closeLibrarySort();});
document.addEventListener('keydown',event=>{
  if($('librarySortMenu').hidden)return;
  if(event.key==='Escape'){closeLibrarySort();$('librarySort').focus();}
  else if(event.key==='ArrowDown'||event.key==='ArrowUp'){
    event.preventDefault();const options=[...$('librarySortMenu').querySelectorAll('button')],i=options.indexOf(document.activeElement);
    options[(i+(event.key==='ArrowDown'?1:-1)+options.length)%options.length].focus();
  }
});
$('documentTitle').oninput=markDocumentDirty;
$('script').addEventListener('input',()=>{markDocumentDirty();count();});$('send').onclick=()=>saveDocument();$('editorPrompt').onclick=promptFromEditor;
$('load').onclick=()=>editingDocument?.title&&openEditor(editingDocument.title);
$('reloadDocument').onclick=()=>editingDocument?.title&&openEditor(editingDocument.title);
$('editorConflictReload').onclick=()=>$('reloadDocument').click();
let resetStyleConfirmPending=false;
$('resetStyle').onclick=async()=>{
  const title=libraryState?.active,version=epoch;
  if(resetStyleConfirmPending||$('resetStyle').disabled||!title)return;
  resetStyleConfirmPending=true;
  try{
    if(!await confirmAction(tr('恢复默认样式？'),tr('将清除当前稿件的独立样式，恢复设备默认阅读设置。稿件正文保持不变。'),{danger:true}))return;
    if(version!==epoch||title!==libraryState?.active)return;
    await libraryAction('正在恢复默认样式…',async()=>{
      const doc=await libraryRequest({op:'get',title});
      await libraryRequest({op:'save',title,text:doc.text,expected:doc.etag,style:null});
      await refreshLibrary();message(tr('已恢复设备默认样式'));
    },'#resetStyle');
  }finally{resetStyleConfirmPending=false;}
};
window.addEventListener('beforeunload',event=>{if(documentDirty){event.preventDefault();event.returnValue='';}});
showPage('home');
try{localStorage.removeItem('teleprompter-draft');localStorage.removeItem('teleprompter-language');}catch{}
