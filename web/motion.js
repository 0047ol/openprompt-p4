// Shared, interruptible motion. No device commands wait for visual effects.
const motionPreference=matchMedia('(prefers-reduced-motion: reduce)');
const motionJobs=new WeakMap(),motionActive=new Set();
let motionNavigation=null;
function motionEnabled(){return !motionPreference.matches&&typeof Element.prototype.animate==='function';}
function motionAnimate(element,frames,options={}){
  if(!element||!motionEnabled())return null;
  motionJobs.get(element)?.cancel();
  const job=element.animate(frames,{duration:180,easing:'cubic-bezier(.2,.7,.2,1)',...options});
  motionJobs.set(element,job);motionActive.add(job);
  const clean=()=>{motionActive.delete(job);if(motionJobs.get(element)===job)motionJobs.delete(element);};
  job.finished.then(clean,clean);
  return job;
}
function finishPageMotion(){
  const current=motionNavigation;motionNavigation=null;
  current?.job.cancel();delete document.documentElement.dataset.pageMotion;
  if(current)document.dispatchEvent(new CustomEvent('page-motion-end',{detail:{element:current.element}}));
}
function motionNavigate(element,direction='forward'){
  finishPageMotion();
  if(!element||!motionEnabled())return null;
  document.documentElement.dataset.pageMotion=direction;
  // Slide the live destination as one opaque page; never copy an outgoing
  // manuscript or scale text, canvases and fixed playback controls.
  const job=motionAnimate(element,[{transform:`translateX(${direction==='back'?'-':''}100%)`},{transform:'translateX(0)'}],{duration:260});
  const current={element,job};motionNavigation=current;
  const finish=()=>{if(motionNavigation===current)finishPageMotion();};
  job.finished.then(finish,finish);
  return job;
}
function motionEnter(element,kind='content',direction='forward'){
  if(kind==='page')return motionNavigate(element,direction);
  const frames=[{opacity:0,transform:kind==='dialog'?'translateY(10px) scale(.98)':'translateY(5px)'},{opacity:1,transform:'none'}];
  return motionAnimate(element,frames,{duration:180});
}
function motionExit(element,kind='content'){
  if(!element||element.hidden||!motionEnabled()||element.getBoundingClientRect().width===0)return;
  // The visual copy cannot receive focus, dispatch commands or duplicate DOM ids.
  // Removing the real content immediately keeps navigation and hidden-state semantics exact.
  const box=element.getBoundingClientRect(),ghost=element.cloneNode(true);
  const originals=[element,...element.querySelectorAll('*')],copies=[ghost,...ghost.querySelectorAll('*')];
  originals.forEach((original,index)=>{
    const copy=copies[index],style=getComputedStyle(original);
    for(const property of style)copy.style.setProperty(property,style.getPropertyValue(property));
    copy.style.animation='none';copy.style.transition='none';
    for(const attribute of [...copy.attributes])if(attribute.name!=='style')copy.removeAttribute(attribute.name);
    if(copy.tagName==='INPUT'||copy.tagName==='BUTTON'||copy.tagName==='SELECT'||copy.tagName==='TEXTAREA')copy.disabled=true;
    if(copy instanceof HTMLCanvasElement){try{copy.getContext('2d').drawImage(original,0,0);}catch{}}
  });
  ghost.classList.add('motion-ghost');ghost.setAttribute('aria-hidden','true');ghost.inert=true;
  if(kind==='library')ghost.dataset.motionScope='library';
  Object.assign(ghost.style,{position:'fixed',left:box.left+'px',top:box.top+'px',width:box.width+'px',height:box.height+'px',margin:'0',pointerEvents:'none',zIndex:kind==='page'?'900':'2100',overflow:'hidden'});
  document.body.append(ghost);
  const frames=kind==='library'?[{opacity:1,transform:'none'},{opacity:0,transform:'translateY(-5px) scale(.98)'}]:[{opacity:1},{opacity:0}];
  const job=motionAnimate(ghost,frames,{duration:kind==='library'?180:kind==='page'?110:120});
  if(job)job.finished.then(()=>ghost.remove(),()=>ghost.remove());else ghost.remove();
}
function motionShow(element,kind='content'){
  const changed=element.hidden;element.hidden=false;
  if(changed)motionEnter(element,kind);
}
function motionHide(element,kind='content'){
  if(!element.hidden)motionExit(element,kind);
  motionJobs.get(element)?.cancel();element.hidden=true;
}
function motionPreferenceChanged(){
  if(!motionPreference.matches)return;
  finishPageMotion();
  for(const job of motionActive)job.cancel();
  document.querySelectorAll('.motion-ghost').forEach(element=>element.remove());
}
if(motionPreference.addEventListener)motionPreference.addEventListener('change',motionPreferenceChanged);
else motionPreference.addListener(motionPreferenceChanged);
document.addEventListener('visibilitychange',()=>{
  if(document.hidden){finishPageMotion();for(const job of motionActive)job.cancel();document.querySelectorAll('.motion-ghost').forEach(element=>element.remove());}
});
// Press feedback lives in CSS :active. Replaying a second scale animation on
// click caused a visible second pulse after the pointer was released.
