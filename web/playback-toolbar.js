// Compact playback actions keep the original controls and protocol handlers.
var playbackToolbarReady=false;
const previewHeading=$('preview').closest('.preview-card').querySelector('.card-head');
const playbackHeading=playbackSettings.querySelector('.card-head'),statusHeading=playbackHeading.querySelector('h3');
const statusGroup=document.createElement('div');statusGroup.className='playback-heading-status';
statusHeading.replaceWith(statusGroup);statusGroup.append($('playState'));
$('playState').setAttribute('aria-level','3');
const pageNumber=document.createElement('span');pageNumber.id='pageNumber';pageNumber.hidden=true;statusGroup.append(pageNumber);
const previewActions=document.createElement('div');previewActions.className='playback-shortcuts';previewHeading.append(previewActions);
const shortcutStore=document.createElement('div');shortcutStore.hidden=true;playbackSettings.append(shortcutStore);
const brightnessPanel=document.createElement('div');brightnessPanel.id='brightnessPanel';brightnessPanel.className='playback-popover';brightnessPanel.setAttribute('popover','auto');brightnessPanel.append($('brightness').closest('.control'));playbackSettings.append(brightnessPanel);
const quickActions=[];
const quickIcons={rotation:'<rect x="6" y="2" width="12" height="20" rx="2"/><path d="m2 7 3-3 3 3m14 10-3 3-3-3"/>',mirror:'<path d="M12 2v20M3 6h6v12H3zm12 0h6v12h-6z"/>',sightGuide:'<path d="M4 8V4h4m8 0h4v4M4 16v4h4m8 0h4v-4M3 12h18"/>',centerHighlight:'<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M3 9h18M3 15h18M8 12h8"/>',loop:'<path d="m17 2 4 4-4 4M3 11V9a3 3 0 0 1 3-3h15M7 22l-4-4 4-4m14-1v2a3 3 0 0 1-3 3H3"/>',autoNext:'<path d="m5 4 10 8-10 8zM19 4v16"/>',brightness:actionIcons.wake,showInfo:'<rect x="3" y="3" width="18" height="18" rx="2"/><path d="M3 7h18M3 17h18"/>'};
function positionPlaybackPopover(panel,anchor){
 const box=anchor.getBoundingClientRect();panel.style.width=Math.min(280,innerWidth-24)+'px';
 panel.style.left=Math.max(12,Math.min(innerWidth-panel.offsetWidth-12,box.right-panel.offsetWidth))+'px';
 panel.style.top=(box.bottom+panel.offsetHeight+12<innerHeight?box.bottom+6:Math.max(12,box.top-panel.offsetHeight-6))+'px';
}
for(const [id,label,target] of [['rotation','切换竖屏',previewActions],['mirror','屏幕镜像',previewActions],['sightGuide','视线提示框',previewActions],['centerHighlight','中心区域高亮',previewActions],['loop','自动循环播放',sheetActions],['autoNext','自动播放下一个',sheetActions],['brightness','屏幕亮度',sheetActions],['showInfo','显示标题和状态栏',sheetActions]]){
 const input=$(id),button=document.createElement('button');button.id=id+'Shortcut';button.type='button';button.className='icon-button';button.dataset.device='';button.dataset.controlKey=id;button.dataset.actionLabel=label;button.dataset.tooltip=label;button.setAttribute('aria-label',tr(label));
 button.innerHTML='<svg viewBox="0 0 24 24" aria-hidden="true">'+quickIcons[id]+'</svg>';
 if(target===sheetActions)target.insertBefore(button,$('resetStyle'));else target.append(button);
 if(id==='brightness'){
  button.setAttribute('aria-haspopup','dialog');button.setAttribute('aria-controls',brightnessPanel.id);
  button.onclick=()=>{if(brightnessPanel.matches(':popover-open'))brightnessPanel.hidePopover();else{brightnessPanel.showPopover();positionPlaybackPopover(brightnessPanel,button);}};
 }else{
  shortcutStore.append(input.closest('label'));
  button.setAttribute('aria-pressed','false');button.onclick=()=>{if(!input.disabled)input.click();};
 }
 quickActions.push({id,input,button});
}
behaviorGroup.before($('pageSecondsControl'));
$('textColor').closest('.control').hidden=true;$('bold').closest('label').hidden=true;
for(const group of [behaviorGroup,appearanceGroup,readingSwitches])group.hidden=true;
// Keep the prompting-mode explanation next to its title, not across the row.
const modeLabel=document.querySelector('label[for=pageMode]'),modeTitle=document.createElement('span');modeTitle.className='field-heading';while(modeLabel.firstChild)modeTitle.append(modeLabel.firstChild);modeLabel.append(modeTitle);
const customSelects=[];
for(const id of ['pageMode','pageTransition']){
 const select=$(id),trigger=document.createElement('button'),menu=document.createElement('div');
 select.hidden=true;trigger.type='button';trigger.className='select-trigger';trigger.dataset.device='';trigger.dataset.controlKey=id;trigger.setAttribute('aria-haspopup','listbox');trigger.setAttribute('aria-expanded','false');trigger.setAttribute('aria-labelledby',id==='pageMode'?'pageModeLabel':'pageTransitionLabel');
 const label=document.querySelector('label[for='+id+']');label.id=id+'Label';label.removeAttribute('for');
 menu.id=id+'Menu';menu.className='select-menu playback-popover';menu.setAttribute('popover','auto');menu.setAttribute('role','listbox');trigger.setAttribute('aria-controls',menu.id);
 for(const option of select.options){const button=document.createElement('button');button.type='button';button.setAttribute('role','option');button.dataset.value=option.value;button.textContent=tr(option.textContent);button.onclick=()=>{select.value=option.value;menu.hidePopover();trigger.focus();select.dispatchEvent(new Event('change',{bubbles:true}));};menu.append(button);}
 select.after(trigger,menu);
 const open=()=>{if(trigger.disabled)return;menu.showPopover();positionPlaybackPopover(menu,trigger);menu.style.width=trigger.getBoundingClientRect().width+'px';menu.style.left=trigger.getBoundingClientRect().left+'px';(menu.querySelector('[aria-selected=true]')||menu.firstElementChild)?.focus();};
 trigger.onclick=()=>menu.matches(':popover-open')?menu.hidePopover():open();
 trigger.onkeydown=e=>{if(['ArrowDown','ArrowUp'].includes(e.key)){e.preventDefault();open();}};
 menu.addEventListener('keydown',e=>{const options=[...menu.children],i=options.indexOf(document.activeElement);if(['ArrowDown','ArrowUp','Home','End'].includes(e.key)){e.preventDefault();options[e.key==='Home'?0:e.key==='End'?options.length-1:(i+(e.key==='ArrowDown'?1:-1)+options.length)%options.length].focus();}if(e.key==='Escape'){e.preventDefault();menu.hidePopover();trigger.focus();}});
 menu.addEventListener('toggle',()=>trigger.setAttribute('aria-expanded',String(menu.matches(':popover-open'))));
 customSelects.push({select,trigger,menu});
}
function renderPlaybackShortcuts(){
 if(!playbackToolbarReady)return;
 const paged=lastState?.pageMode===1;
 for(const {id,input,button} of quickActions){
  setControlDisabled(button,input.disabled,input.hasAttribute('data-pending'));button.hidden=paged&&(id==='loop'||id==='autoNext');
  if(input.type==='checkbox')button.setAttribute('aria-pressed',String(input.checked));
  button.setAttribute('aria-label',tr(button.dataset.actionLabel));
 }
 for(const {select,trigger,menu} of customSelects){
  setControlDisabled(trigger,select.disabled);trigger.textContent=tr(select.selectedOptions[0].textContent);
  for(const option of menu.children){option.textContent=tr([...select.options].find(x=>x.value===option.dataset.value).textContent);option.setAttribute('aria-selected',String(option.dataset.value===select.value));}
 }
}
playbackToolbarReady=true;
window.addEventListener('resize',()=>{for(const panel of [brightnessPanel,...customSelects.map(x=>x.menu)])if(panel.matches(':popover-open'))panel.hidePopover();});
