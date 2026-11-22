// Row spans are authoritative on the device; old firmware keeps fixed-size pages.
function readingPage(layout,index=0){
  return layout.pages?.[index]||[index*(layout.pageRows||1),layout.pageRows||1];
}
function readingPageOffset(layout,index=0){
  return readingPage(layout,index)[0]*(layout.lineHeight+layout.lineSpacing);
}
// Used by both visual Demos. Byte offsets match the firmware's wrapped lines.
function buildReadingPages(lines,text,rows,capacity,paragraphs){
  const data=new TextEncoder().encode(text),decode=new TextDecoder(),pages=[];
  for(let row=0;row<rows;++row){
    const [start,length]=lines[row];
    if(paragraphs&&!/[^\x00-\x20]/.test(decode.decode(data.slice(start,start+length))))continue;
    let page=pages[pages.length-1];
    if(!page||page[1]>=capacity||page[0]+page[1]!==row){page=[row,0];pages.push(page);}
    ++page[1];
  }
  return pages.length?pages:[[0,1]];
}
