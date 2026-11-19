/* Deep links for visual review only. All actions use disposable Demo data. */
(()=>{
 const view=new URLSearchParams(location.search).get('view');
 if(!view)return;
 let attempts=0;
 const ready=setInterval(()=>{
  if(++attempts>150){clearInterval(ready);return;}
  if(typeof online==='undefined'||!online||typeof libraryState==='undefined'||!libraryState?.items.length||busy||libraryActionPending)return;
  clearInterval(ready);
  const first=selector=>document.querySelector(selector)?.click();
  if(view==='play')void openPlayback(libraryState.items[0].title);
  else if(view==='edit')void openEditor(libraryState.items[0].title);
  else if(view==='new')void openEditor(null);
  else if(view==='settings')void leaveEditor('settings');
  else if(view==='list'&&libraryView.layout!==1)first('#libraryLayout');
  else if(view==='cards'&&libraryView.layout!==0)first('#libraryLayout');
  else if(view==='offline')TP_DEMO.disconnect();
 },100);
})();
