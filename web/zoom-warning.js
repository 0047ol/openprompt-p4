/* Browsers expose pinch zoom, but no standard desktop page-zoom value.
 * Use conservative window geometry for desktop, never devicePixelRatio alone:
 * OS scaling and Retina screens must not trigger a warning. */
function detectPageZoom({scale=1,outerWidth,outerHeight,innerWidth,innerHeight,handheld=false,native=false}){
  if(native)return null;
  if(Number.isFinite(scale)&&Math.abs(scale-1)>.07)return {kind:'pinch',ratio:scale};
  if(handheld||!outerWidth||!innerWidth||!outerHeight||!innerHeight)return null;
  const ratio=outerWidth/innerWidth;
  if(ratio<.45||ratio>5.1||Math.abs(ratio-1)<.07)return null;
  const levels=[.5,.67,.75,.8,.9,1.1,1.25,1.5,1.75,2,2.5,3,4,5];
  const nearest=levels.reduce((a,b)=>Math.abs(b-ratio)<Math.abs(a-ratio)?b:a);
  // Docked side panels/devtools also reduce innerWidth. Their inconsistent
  // vertical geometry is not evidence of page zoom, so do not warn for them.
  const chrome=outerHeight-innerHeight*nearest;
  if(Math.abs(nearest-ratio)>.025*nearest||chrome<30||chrome>320)return null;
  return {kind:'desktop',ratio:nearest};
}
(function installZoomWarning(){
  let timer=0,lastKey='',suppressed=false;
  try{suppressed=sessionStorage.getItem('tp-zoom-warning-dismissed')==='1';}catch{}
  const dialog=document.createElement('dialog');dialog.id='zoomWarning';dialog.setAttribute('aria-labelledby','zoomWarningTitle');
  dialog.innerHTML='<h3 id="zoomWarningTitle"></h3><p id="zoomWarningText"></p><label class="check"><input id="zoomWarningMute" type="checkbox"><span></span></label><form method="dialog" class="actions"><button class="primary" value="close"></button></form>';
  document.body.append(dialog);
  function copy(){
    const en=uiLanguage==='en';
    dialog.querySelector('h3').textContent=en?'Check page zoom':'页面缩放提示';
    dialog.querySelector('p').textContent=en?'The page appears to be zoomed. This can change the layout, settings drawer and preview size. For consistent display, restore browser zoom to 100% (Ctrl+0, or ⌘+0 on Mac); on a phone, pinch back to the original size.':'检测到页面可能处于缩放状态，可能影响布局、设置抽屉和预览比例。建议恢复至 100% 后再使用：电脑按 Ctrl+0（Mac 按 ⌘+0），手机可双指缩回原始大小。';
    dialog.querySelector('label span').textContent=en?'Do not remind me again in this tab':'本次不再提醒';
    dialog.querySelector('button').textContent=en?'Got it':'我知道了';
  }
  function dismiss(){
    if(dialog.querySelector('input').checked){suppressed=true;try{sessionStorage.setItem('tp-zoom-warning-dismissed','1');}catch{}}
    dialog.close();
  }
  dialog.querySelector('form').addEventListener('submit',event=>{event.preventDefault();dismiss();});
  dialog.addEventListener('cancel',event=>{event.preventDefault();dismiss();});
  function check(){
    if(document.hidden||suppressed)return;
    const handheld=/Android|iPhone|iPad|iPod/.test(navigator.userAgent)||(/Mac/.test(navigator.platform)&&navigator.maxTouchPoints>1);
    const result=detectPageZoom({scale:window.visualViewport?.scale??1,outerWidth,outerHeight,innerWidth,innerHeight,handheld,native:!!nativeApp});
    if(!result){if(dialog.open)dialog.close();return;}
    if(dialog.open){copy();return;}
    const key=result.kind+':'+result.ratio;
    if(lastKey===key)return;
    if(document.querySelector('dialog[open]')||firmwareLocked()){schedule();return;}
    lastKey=key;copy();dialog.showModal();motionEnter(dialog,'dialog');
  }
  function schedule(){clearTimeout(timer);timer=setTimeout(check,700);}
  addEventListener('resize',schedule);addEventListener('pageshow',schedule);addEventListener('languagechange',schedule);
  window.visualViewport?.addEventListener('resize',schedule);document.addEventListener('visibilitychange',schedule);
  schedule();
})();
