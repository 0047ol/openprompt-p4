/* Both sources use the exact device glyph outlines and metrics. Local pages
 * carry an index so an offline first visit never depends on an internet fetch. */
const readerFontSource=/* READER_CDN_SOURCE */;
const readerOfflineIndex=/* READER_OFFLINE_INDEX */;
let readerUseLocal=location.protocol==='http:'&&location.hostname==='192.168.4.1';
const readerLocalAvailable=location.protocol==='http:';
let readerLocalLoads=Promise.resolve();
let readerFontManifest=null,readerFontManifestLoading=null;
const readerFontFiles=new Map();

let readerFontSupplement=null;
const readerFontByCodepoint=new Map();
async function readerLoadManifest(){
  if(!readerFontManifestLoading)readerFontManifestLoading=(async()=>{
    if(window.teleprompterNative?.fontBase||readerLocalAvailable)readerFontManifest=readerOfflineIndex;
    else if(typeof readerFontSource==='object')readerFontManifest=readerFontSource;
    else{
      const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),4000);
      try{
        const response=await fetch(readerFontSource,{signal:controller.signal,credentials:'omit',referrerPolicy:'no-referrer',cache:'force-cache'});
        if(!response.ok)throw Error('Font index unavailable');
        readerFontManifest=await response.json();
      }finally{clearTimeout(timer);}
    }
    for(const file of readerFontManifest.files){
      if(typeof file.points==='string')file.points=Array.from(file.points,char=>char.codePointAt(0));
      for(const cp of file.points)readerFontByCodepoint.set(cp,file);
    }
  })().catch(error=>{readerFontManifestLoading=null;throw error;});
  return readerFontManifestLoading;
}
async function readerLoadShard(file){
  if(!readerFontFiles.has(file.file))readerFontFiles.set(file.file,(async()=>{
    if(readerLocalAvailable&&typeof wifiState!=='undefined'&&wifiState?.apEnabled!==false&&
       (location.hostname===wifiState?.apIp||(!wifiState?.connected&&location.hostname===wifiState?.localDomain)))readerUseLocal=true;
    const mirror='https://0047ol.github.io/openprompt-p4/fonts/noto-sc-4.5.12/'+file.file;
    const local='/reader.woff2?shard='+readerFontManifest.files.indexOf(file)+'&v='+readerFontManifest.sourceSha256.slice(0,12);
    const bundled=window.teleprompterNative?.fontBase;
    // The controlled mirror shares the already-reached controller origin.
    // Keep the CDN as a public-page fallback and the P4 as the LAN fallback.
    const sources=bundled?[bundled+file.file]:readerUseLocal?[local]:readerLocalAvailable?[mirror,local]:[mirror,file.url];
    for(const [index,url] of sources.entries()){
      const isLocal=url===local;
      let release;
      if(isLocal){const previous=readerLocalLoads;readerLocalLoads=new Promise(resolve=>{release=resolve;});await previous;}
      const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),isLocal?15000:readerLocalAvailable?1500:5000);
      try{
        const response=await fetch(url,{signal:controller.signal,credentials:'omit',referrerPolicy:'no-referrer',cache:'force-cache'});
        if(!response.ok)throw Error('Font source unavailable');
        const data=await response.arrayBuffer();
        const face=new FontFace('DeviceReader',data,{unicodeRange:file.points.map(cp=>'U+'+cp.toString(16)).join(',')});
        await face.load();document.fonts.add(face);if(isLocal)readerUseLocal=true;return;
      }catch(error){if(index===sources.length-1)throw error;}
      finally{clearTimeout(timer);release?.();}
    }
  })().catch(error=>{readerFontFiles.delete(file.file);throw error;}));
  return readerFontFiles.get(file.file);
}
async function readerLoadSupplement(){
  if(!readerFontSupplement)readerFontSupplement=(async()=>{
    const bytes=Uint8Array.from(atob(readerFontManifest.supplement),c=>c.charCodeAt(0));
    const face=new FontFace('DeviceReader',bytes,{unicodeRange:readerFontManifest.localOnly.map(cp=>'U+'+cp.toString(16)).join(',')});
    await face.load();document.fonts.add(face);
  })();
  return readerFontSupplement;
}
async function loadReaderTextFont(text){
  await readerLoadManifest();
  const files=new Set();let supplement=false;
  for(const char of text){
    const cp=char.codePointAt(0);if(cp<32)continue;
    const file=readerFontByCodepoint.get(cp);
    if(file)files.add(file);
    else if(readerFontManifest.localOnly.includes(cp))supplement=true;
    // The device has the same limited repertoire. Never replace it with a
    // system font or download a different whole font for missing codepoints.
  }
  if(supplement)await readerLoadSupplement();
  const pending=[...files];let index=0;
  const results=await Promise.allSettled(Array.from({length:Math.min(4,pending.length)},async()=>{
    while(index<pending.length)await readerLoadShard(pending[index++]);
  }));
  const failed=results.find(result=>result.status==='rejected');
  if(failed)throw failed.reason;
}
