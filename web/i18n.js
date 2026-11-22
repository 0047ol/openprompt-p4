'use strict';
const translations=/* TRANSLATIONS_JSON */;
var webProductVersion=/* PRODUCT_VERSION */;
function updateProductTitle(){
  const text=tr('提词器P4')+' ('+webProductVersion+')';document.title=text;
  const heading=document.getElementById('productTitle');if(heading){const version=document.createElement('span');version.className='product-version';version.textContent='('+webProductVersion+')';heading.replaceChildren(document.createTextNode(tr('提词器P4')+' '),version);}
}
const browserLanguage=()=>String(navigator.languages?.[0]||navigator.language||'zh-CN').toLowerCase().startsWith('zh')?'zh-CN':'en';
let uiLanguage=browserLanguage();
function tr(text){return uiLanguage==='en'?(translations[text]??text):text;}
const localizedNodes=[],localizedAttributes=[];
const textWalker=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT);
while(textWalker.nextNode()){
  const node=textWalker.currentNode,source=node.textContent,trimmed=source.trim();
  if(translations[trimmed] && !['SCRIPT','STYLE'].includes(node.parentElement?.tagName))localizedNodes.push({node,source,trimmed});
}
for(const el of document.querySelectorAll('[placeholder],[title],[aria-label],[label]'))for(const name of ['placeholder','title','aria-label','label']){
  const source=el.getAttribute(name);if(translations[source])localizedAttributes.push({el,name,source});
}
function applyLanguage(language){
  uiLanguage=language==='en'?'en':'zh-CN';document.documentElement.lang=uiLanguage;
  for(const {node,source,trimmed} of localizedNodes)if(node.isConnected)node.textContent=source.replace(trimmed,tr(trimmed));
  for(const {el,name,source} of localizedAttributes)el.setAttribute(name,tr(source));
  updateProductTitle();
}
const colorPreference=matchMedia('(prefers-color-scheme: light)');
function applySystemTheme(){
  const light=matchMedia('(prefers-color-scheme: light)').matches;
  const theme=light?'light':'dark';
  if(document.documentElement.dataset.theme!==theme)document.documentElement.dataset.theme=theme;
  document.querySelector('meta[name="color-scheme"]').content='only '+theme;
  document.querySelector('meta[name="theme-color"]').content=light?'#f2f5f0':'#10171c';
}
applySystemTheme();
if(colorPreference.addEventListener)colorPreference.addEventListener('change',applySystemTheme);
else colorPreference.addListener?.(applySystemTheme);
function refreshSystemPreferences(){
  applySystemTheme();
  const language=browserLanguage();
  if(language!==uiLanguage){applyLanguage(language);if(typeof refreshLanguage==='function')refreshLanguage();}
}
window.addEventListener('languagechange',refreshSystemPreferences);
window.addEventListener('pageshow',refreshSystemPreferences);
window.addEventListener('focus',refreshSystemPreferences);
document.addEventListener('visibilitychange',()=>{if(!document.hidden)refreshSystemPreferences();});
