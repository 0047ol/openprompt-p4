/* Production countdown model under delayed/duplicate packets and page changes. */
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('web/preview.js','utf8');
const context={};vm.createContext(context);
vm.runInContext(source.slice(source.indexOf('function createPreviewPageTimer'),source.indexOf('const previewPageTimer=')),context);
const base={boot:1,scriptRevision:1,rewindRevision:1,previewRevision:1,pageMode:1,pageCount:1,pageIndex:0,pageChangedMs:0,pageSeconds:1,loopCount:0,playing:true,pageRemainingMs:1000};
const clock=context.createPreviewPageTimer();
clock.update(base,100);
assert.equal(clock.remaining(600),500);
clock.update({...base,pageRemainingMs:400},800);
assert.equal(clock.remaining(1000),100,'late sample cannot extend the interval');
assert.equal(clock.remaining(1200),0);
clock.update({...base,pageRemainingMs:200},1300);
assert.equal(clock.remaining(1300),0,'expired same page cannot jump back to 1');
clock.update({...base,playing:false,stopped:true,playbackFinished:true,pageRemainingMs:1000,rewindRevision:2},1400);
assert.equal(clock.remaining(3000),0,'automatic stop displays zero despite resetting the next play interval');
clock.update({...base,rewindRevision:2},3100);
assert.equal(clock.remaining(3400),700,'replay receives a full interval');
clock.update({...base,rewindRevision:2,playing:false,pageRemainingMs:600},3500);
assert.equal(clock.remaining(9000),600,'pause freezes the reported remainder');
clock.update({...base,rewindRevision:2,pageRemainingMs:600},9100);
assert.equal(clock.remaining(9400),300,'resume counts only the unspent interval');
for(const delta of [{pageIndex:1,pageCount:2,pageChangedMs:9400},{previewRevision:2},{pageSeconds:2,pageRemainingMs:2000},{scriptRevision:2},{boot:2},{loopCount:1}]){
 const timer=context.createPreviewPageTimer();timer.update(base,100);timer.update({...base,...delta},1200);
 assert.equal(timer.remaining(1200),delta.pageRemainingMs||1000,'new interval '+JSON.stringify(delta));
}
console.log('PASS one-second countdown, late packets, final stop, replay, pause/resume, page/reflow/config/boot/cycle changes');
