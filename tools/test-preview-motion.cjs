/* Deterministic packet jitter and rendering clock; uses the production model. */
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('web/preview.js','utf8');
const context={};vm.createContext(context);
vm.runInContext(source.slice(0,source.indexOf('const previewMotion=')),context);
const state={boot:1,previewRevision:1,rewindRevision:1,loopCount:0,scroll:0,playing:true,speed:100,shuttle:0,countdownRemainingMs:0};
for(const stamped of [false,true]){
 const motion=context.createPreviewMotion();let previous=0,maxStep=0;
 const packets=[{at:0,sample:0},{at:550,sample:500},{at:1250,sample:1000},{at:1520,sample:1500},{at:2250,sample:2000},{at:2510,sample:2500}];
 let packet=0;
 for(let now=0;now<=3000;now+=10){
  while(packet<packets.length&&packets[packet].at<=now){
   const p=packets[packet++];motion.update({...state,scroll:p.sample/10,...(stamped?{scrollSampleMs:p.sample}:{})},now);
  }
  const position=motion.position(now),step=position-previous;
  assert(step>=-1e-9,`forward jumped backwards at ${now}: ${step}`);
  assert(step<=1.51,`packet arrival caused jump at ${now}: ${step}`);
  maxStep=Math.max(maxStep,step);previous=position;
 }
 assert(Math.abs(previous-300)<30,`must still follow device position: ${previous}`);
 motion.update({...state,loop:true,loopPeriod:300,scroll:0,loopCount:1},3010);assert.equal(motion.position(3010),300,'loop seam preserves unwrapped continuous position');
 motion.update({...state,scroll:199,rewindRevision:2},3020);assert.equal(motion.position(3020),0,'explicit stop/rewind overrides stale position');
 motion.update({...state,scroll:180,rewindRevision:2,playing:false},3030);assert.equal(motion.position(3200),180,'pause holds confirmed position');
 console.log(`PASS ${stamped?'timestamped':'legacy'} status: jittered packets stay monotonic, max 10 ms step ${maxStep.toFixed(3)} px, follow position, loop/rewind/pause`);
}
const reverse=context.createPreviewMotion();reverse.update({...state,scroll:1000,shuttle:-1},0);let previous=1000;
for(let now=10;now<=2000;now+=10){
 if(now%500===0)reverse.update({...state,scroll:1000-now/10+20,shuttle:-1},now);
 const next=reverse.position(now);assert(next<=previous+1e-9,'reverse jumped forwards');previous=next;
}
console.log('PASS reverse playback remains monotonic under delayed position reports');
{
 const motion=context.createPreviewMotion();
 const looping={...state,scriptRevision:5,loop:true,loopPeriod:400,loopCount:2,scroll:350};
 motion.update(looping,0);
 assert.equal(motion.position(200),1170);
 motion.update({...looping,previewRevision:2,loopPeriod:undefined,scroll:370},200);
 assert.equal(motion.position(200),1170,'A pending loop layout must retain the confirmed unwrapped coordinate');
 motion.update({...looping,previewRevision:2,loopPeriod:undefined,loopCount:3,scroll:10},600);
 assert.equal(motion.position(600),1210,'A loop seam while layout is pending must remain continuous');
 motion.update({...looping,previewRevision:2,loopCount:3,scroll:30},800);
 assert.equal(motion.position(800),1230,'Receiving the same loop period must not jump again');
 motion.update({...looping,previewRevision:2,loopPeriod:undefined,scroll:0,loopCount:0,playing:false,rewindRevision:2},900);
 assert.equal(motion.position(900),0,'Stop still resets while the layout is pending');
 motion.update({...looping,scriptRevision:6,loopPeriod:undefined,loopCount:0,scroll:0},1000);
 assert.equal(motion.position(1000),0,'A new manuscript must not reuse the previous loop coordinate');
 console.log('PASS delayed looping layout preserves coordinates across missing period, seam, restore and stop; new manuscript resets');
}
{
 const motion=context.createPreviewMotion();
 motion.update({...state,scroll:0,loopPeriod:400},0);
 const before=motion.position(1200);
 motion.update({...state,previewRevision:2,scroll:0,loopPeriod:500},1200);
 assert.equal(motion.position(1200),before,'Delayed layout publication must not reset uninterrupted modern playback');
 assert(motion.position(1400)>=before,'Layout correction cannot move forward text backwards');
 motion.update({...state,previewRevision:3,scroll:0,rewindRevision:2},1500);
 assert.equal(motion.position(1500),0,'Explicit device rewind still resets across layout changes');
 const legacy=context.createPreviewMotion();
 legacy.update({...state,rewindRevision:undefined},0);
 legacy.update({...state,rewindRevision:undefined,previewRevision:2,scroll:20},1000);
 assert.equal(legacy.position(1000),20,'Legacy device layout reset remains supported');
 console.log('PASS delayed layout publication stays continuous; explicit and legacy resets retained');
}
for(const playbackFinished of [false,true]){
 const stopped=context.createPreviewMotion();
 stopped.update({...state,scroll:500,rewindRevision:1},0);
 stopped.update({...state,scroll:0,playing:false,stopped:true,playbackFinished,rewindRevision:2},100);
 assert.equal(stopped.position(100),0,'A stop immediately resets the rendered position');
 assert.equal(stopped.position(5000),0,'Stopped text must not continue moving');
 stopped.update({...state,scroll:0,playing:false,stopped:true,playbackFinished,rewindRevision:2},5100);
 assert.equal(stopped.position(5200),0,'Repeated stop packets cannot bounce back');
 stopped.update({...state,scroll:0,playing:false,rewindRevision:2,countdownRemainingMs:1000},5300);
 assert.equal(stopped.position(5400),0,'Replay countdown remains at the start');
 stopped.update({...state,scroll:0,playing:true,rewindRevision:2},5500);
 assert.equal(stopped.position(5600),10,'Playback resumes from the beginning');
}
console.log('PASS manual/automatic stop resets immediately, remains still and replays from start');
