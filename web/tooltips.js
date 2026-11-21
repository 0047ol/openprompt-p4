// One tooltip surface, including controls created later by the document library.
const tooltip=$('tooltip');let tooltipOwner=null,tooltipTimer,tooltipPinned=false;
function hideTooltip(){
  clearTimeout(tooltipTimer);motionHide(tooltip);tooltipPinned=false;
  if(tooltipOwner?.classList.contains('info-button'))tooltipOwner.setAttribute('aria-expanded','false');
  if(tooltipOwner){
    const ids=(tooltipOwner.getAttribute('aria-describedby')||'').split(' ').filter(id=>id&&id!=='tooltip');
    if(ids.length)tooltipOwner.setAttribute('aria-describedby',ids.join(' '));else tooltipOwner.removeAttribute('aria-describedby');
  }
  tooltipOwner=null;
}
function showTooltip(owner,pinned=false){
  if(!owner)return;hideTooltip();tooltipOwner=owner;tooltipPinned=pinned;
  if(owner.classList.contains('info-button'))owner.setAttribute('aria-expanded','true');
  tooltipTimer=setTimeout(()=>{
    if(!owner.isConnected)return;
    let text=owner.dataset.tooltip||owner.getAttribute('title')||owner.getAttribute('aria-label');
    if(!text)return;
    if(owner.hasAttribute('title')){owner.dataset.tooltip=text;owner.removeAttribute('title');}
    tooltip.textContent=tr(text);tooltip.hidden=false;
    owner.setAttribute('aria-describedby',((owner.getAttribute('aria-describedby')||'')+' tooltip').trim());
    const box=owner.getBoundingClientRect(),tip=tooltip.getBoundingClientRect();
    tooltip.style.left=Math.max(8,Math.min(innerWidth-tip.width-8,box.left+(box.width-tip.width)/2))+'px';
    tooltip.style.top=(box.top>tip.height+16?box.top-tip.height-8:Math.min(innerHeight-tip.height-8,box.bottom+8))+'px';
    motionEnter(tooltip);
  },pinned?0:200);
}
const help={
  showHeader:'显示屏幕顶部的信息栏；待机引导页始终保留标题栏。',
  showFooter:'显示屏幕底部的信息栏；待机引导页始终隐藏状态栏。',
  headerHeight:'同时调整标题栏高度和状态栏高度，最小 24 像素。正文第一行保持屏幕正中央；增加栏高不会增加状态栏的单行容量。',
  headerFontSize:'同时调整标题栏和状态栏字号；高度不足时自动缩小到安全范围。状态栏最多显示 6 项，空间不足时请减小字号或减少项目。',
  firmwareOnlineAutomatic:'默认开启。浏览器联网且设备连接正常时检查服务器版本，每次打开控制台发现新版时提示一次；不会自动下载或安装。',
  sdMount:'重新挂载已弹出或重新插入的 SD 卡。',
  hotspotLink:'修改后自动保存热点 IP，可能需要重新连接热点。热点地址固定，子网掩码为 255.255.255.0。',
  lanLink:'修改后自动保存为固定 IP，沿用当前网络的网关、子网掩码和 DNS。请使用同网段内未被其他设备占用的地址；旁边按钮可恢复自动获取。',
  networkHostname:'输入字母、数字或连字符，自动添加 .local，修改后单独保存，不改变 IP。通过 mDNS 访问；无法解析时仍可使用 IP 地址。',
  networkAuto:'恢复由路由器自动分配 IP，保留已保存的本地域名。',
  sdEject:'停止播放并卸载 SD 卡，完成后可取出。',
  sdFormat:'清空整张 SD 卡上的所有文件，无法恢复。',
  count:'每篇正文最多 8 KB（8192 个 UTF-8 字节），约 2700 个常见汉字；标点、换行和表情也占容量。这不是 SD 卡总容量。',
  firmwareAutomatic:'启动时检查 SD 卡的 firmware 文件夹，只安装较新版本；已安装或曾启动失败的版本不会重复更新。',
  firmwareLocal:'选择电脑或手机中的固件文件，确认版本后更新设备。',
  firmwareScan:'从 SD 卡的 firmware 文件夹查找新版固件，确认版本后安装；不会格式化或检测 SD 卡健康状况。',
  backward:'倒退：每次确认后点击，依次切换 ×1 至 ×8，再回到 ×1；倒计时期间不可用。点击播放恢复正常速度。',
  forward:'快进：每次确认后点击，依次切换 ×1 至 ×8，再回到 ×1；倒计时期间不可用。点击播放恢复正常速度。',
  rewind:'停止并回到开头；再次播放时按设置倒计时。',
  nextDocument:'按稿件库顺序切换到下一篇并停止，点击播放后开始；最后一篇时不可用。',
  previousDocument:'按稿件库顺序切换到上一篇并停止，点击播放后开始；第一篇时不可用。',
  speed:'正常播放的滚动速度；快进和倒退倍率独立计算。',
  font:'调整屏幕文字大小，可拖动滑块或直接输入数值。',
  bold:'加粗正文笔画，保持字号、字间距和换行位置。',
  letterSpacing:'相邻字形边缘之间的距离，0 表示贴紧；正文中的空格仍会保留。',lineSpacing:'两行字形区域之间的距离，0 表示贴紧，不包含字体自带留白。',
  countdown:'从开头播放时倒计时；暂停后继续播放不会重复倒计时。',
  brightness:'正常亮度最低为 10%；休眠会关闭背光，唤醒后恢复。',
  rotation:'横屏以充电口朝下为正向，竖屏以充电口朝右为正向。',
  mirror:'为提词玻璃上下翻转正文与倒计时，设备状态提示保持正常方向。',
  sightGuide:'标记屏幕正中央的阅读位置，正文第一行从这里开始。',
  centerHighlight:'突出阅读提示区域，降低区域外文字亮度。',
  loop:'正文播放结束后自动从开头继续；开启后关闭自动播放下一篇。',
  autoNext:'当前稿件播完后，按稿件库排序播放下一篇，并使用下一篇的样式和倒计时；开启后关闭循环，最后一篇播完停止。',
  marginLeft:'同时调整正文左右两侧的留白。',textAlign:'设置正文的左对齐、居中或右对齐。',
  textColor:'设置正文和播放倒计时颜色。',resetStyle:'将当前稿件的阅读样式恢复为设备默认值。',
  autoSleepSeconds:'暂停且无操作达到此时间后休眠；0 表示关闭自动休眠。',
  autoShutdownSeconds:'无操作达到此时间后自动关机；0 表示关闭。播放和升级期间不计时，关机后需按 RST 或重新供电。',
  showInfo:'此稿件的显示设置优先于设备默认值，自动保存到当前稿件库；恢复默认样式后使用设备设置。',
  bleConnect:'首次连接需在浏览器中选择设备，之后会尝试自动重连。',
  deviceSleep:'息屏',deviceWake:'唤醒',deviceShutdown:'关机',deviceRestart:'重启'
};
for(const bit of [32,256]){
  const input=document.querySelector('[data-bar-bit="'+bit+'"]');
  if(input)input.closest('label').dataset.tooltip='帧率是正文实际提交帧数；抖动是提交帧间隔的标准差，均按两秒窗口统计。';
}
const bufferOption=document.querySelector('[data-bar-bit="128"]');
if(bufferOption)bufferOption.closest('label').dataset.tooltip='已就绪的文字分块数 / 分块缓冲容量，不代表整篇稿件已缓存。';
for(const [bit,tip] of Object.entries({16:'相对于当前速度的播放倍率，正常播放为 x1；快进和回退图标表示方向。',4096:'PSRAM 可用容量 / 总容量，不包含独立的内部 RAM。',65536:'设备热点接口的 MAC 地址。',131072:'显示当前设备热点名称，与同栏其他文字使用相同字号和颜色。',2048:'仅在循环播放开启时显示已完成的循环次数。',8:'仅在自动播放下一篇开启且存在下一篇稿件时显示。',8388608:'不循环时，按稿件库当前顺序显示上一篇稿件。',4194304:'按正文实际提交帧率、区域大小和 RGB565 每像素 16 位计算的等效像素码率；不是网络流量或压缩视频码率。',16777216:'本次开机以来成功提交的正文绘制帧数。',33554432:'正文连续播放时超过预期提交间隔的帧数；考虑低速下的整数像素步进，不代表 LCD 硬件丢帧。'})){
  const input=document.querySelector(`[data-bar-bit="${bit}"]`);if(input)input.closest('label').dataset.tooltip=tip;
}
for(const [id,text] of Object.entries(help)){
  const el=$(id);if(!el)continue;
  const owner=el.matches('input[type=checkbox]')?el.closest('label'):el;
  owner.dataset.tooltip=text;
  if(owner.tagName==='LABEL')owner.tabIndex=0;
  if($(id+'Number'))$(id+'Number').dataset.tooltip=text;
}
for(const small of document.querySelectorAll('#deviceSettingsForm small')){
  const label=small.closest('label'),input=label?.querySelector('input');
  if(input)input.dataset.tooltip=(input.dataset.tooltip?input.dataset.tooltip+' ':'')+small.textContent;
  small.hidden=true;
}
for(const [id,text] of Object.entries(help)){
  const input=$(id);if(!input||input.tagName==='BUTTON')continue;
  const label=id==='count'?input.parentElement:document.querySelector('label[for="'+id+'"]')||input.closest('label');if(!label)continue;
  // Explicit help already belongs beside this label. Reuse it instead of
  // creating a second icon or leaving the existing one over the switch.
  const existing=label.querySelector('.info-button');
  if(existing){existing.dataset.tooltip=[existing.dataset.tooltip,text].filter(Boolean).join(' ');continue;}
  const info=document.createElement('button');info.type='button';info.className='info-button';info.textContent='ⓘ';
  info.setAttribute('aria-label',tr('说明'));info.setAttribute('aria-expanded','false');info.dataset.tooltip=text;
  // Prevent a help tap from toggling its enclosing checkbox/label.
  info.addEventListener('click',e=>{e.preventDefault();e.stopPropagation();if(tooltipPinned&&tooltipOwner===info)hideTooltip();else showTooltip(info,true);});
  if(id==='count'){label.append(info);continue;}
  let heading=label.querySelector('.bar-height-name,.setting-field-title,.switch-text');
  if(!heading)heading=label.querySelector(':scope > span:not(.bar-height-value)');
  if(!heading){
    heading=document.createElement('span');
    const textNodes=[...label.childNodes].filter(node=>node.nodeType===Node.TEXT_NODE&&node.textContent.trim());
    if(textNodes.length){label.insertBefore(heading,textNodes[0]);for(const node of textNodes)heading.append(node);}
    else label.prepend(heading);
  }
  heading.classList.add('field-heading');
  const titleText=document.createElement('span');titleText.className='field-label-text';
  while(heading.firstChild)titleText.append(heading.firstChild);
  heading.append(titleText,info);
}
for(const info of document.querySelectorAll('.info-button[data-tooltip]:not([aria-expanded])')){
  info.setAttribute('aria-expanded','false');
  info.addEventListener('click',e=>{e.preventDefault();e.stopPropagation();if(tooltipPinned&&tooltipOwner===info)hideTooltip();else showTooltip(info,true);});
}
// Use the same circular outline on every platform, independent of glyph fonts.
for(const info of document.querySelectorAll('.info-button')){
  info.innerHTML='<svg viewBox="0 0 20 20" aria-hidden="true" focusable="false"><circle cx="10" cy="10" r="8"/><path d="M10 9v5m0-8v.1"/></svg>';
}
function tooltipTarget(target){return target.closest('[data-tooltip],[title],button[aria-label]');}
document.addEventListener('pointerover',e=>{if(tooltipPinned||e.pointerType==='touch')return;const owner=tooltipTarget(e.target);if(owner!==tooltipOwner)showTooltip(owner);});
document.addEventListener('pointerout',e=>{if(!tooltipPinned&&tooltipOwner&&!tooltipOwner.contains(e.relatedTarget))hideTooltip();});
document.addEventListener('focusin',e=>{if(!tooltipPinned&&e.target.matches(':focus-visible')&&!e.target.classList.contains('info-button'))showTooltip(tooltipTarget(e.target));});
document.addEventListener('focusout',()=>{if(!tooltipPinned)hideTooltip();});
document.addEventListener('keydown',e=>{if(e.key==='Escape')hideTooltip();});
document.addEventListener('pointerdown',e=>{if(e.target.closest('.info-button'))return;hideTooltip();});
// A touch hold explains icon-only actions without performing them.
let iconHoldTimer,heldIcon=null;
document.addEventListener('pointerdown',e=>{
  const button=e.target.closest('.icon-button');if(e.pointerType!=='touch'||!button)return;
  clearTimeout(iconHoldTimer);heldIcon=null;
  iconHoldTimer=setTimeout(()=>{heldIcon=button;showTooltip(button,true);},450);
});
document.addEventListener('pointerup',()=>clearTimeout(iconHoldTimer));
document.addEventListener('pointercancel',()=>{clearTimeout(iconHoldTimer);heldIcon=null;});
document.addEventListener('pointermove',e=>{if(e.pointerType==='touch')clearTimeout(iconHoldTimer);});
document.addEventListener('click',e=>{if(heldIcon&&heldIcon.contains(e.target)){e.preventDefault();e.stopImmediatePropagation();heldIcon=null;}},true);
document.addEventListener('scroll',hideTooltip,true);
window.addEventListener('resize',hideTooltip);
