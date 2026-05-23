pull();var pollTid=setInterval(function(){if(!document.hidden)pull()},5000);
(function(){
  var ws,wst,wsDelay=3000;
  function applyState(x){
    pullFails=0;hideOffline();
    // Skip updating a slider that is currently being dragged — avoids the thumb
    // snapping back to the server-confirmed value while the user is still moving it.
    var ae=document.activeElement;
    if(ae!==sb)pb(x.b);
    if(ae!==sc)pc(x.c);
    if(ae!==sco)pco(x.co);
    if(ae!==ssp)psp(x.sp);
    if(x.bl!==undefined&&ae!==sbl)pbl(x.bl);
    if(x.th!==undefined)pth(x.th);
    if(x.w!==undefined)document.getElementById('vw').textContent=x.w.toFixed(1);
    if(x.upd&&!document.getElementById('chk').disabled){var vi=document.getElementById('vinfo');if(!vi.textContent){vi.style.color='#fbbf24';vi.textContent=ru?'● Доступно обновление':'● Update available';}}
    if(x.name!==undefined&&aiPending){
      aiPending=false;clearTimeout(aiTimeout);aiTimeout=null;
      var sbtn=document.getElementById('surprise'),snm=document.getElementById('ainame');
      sbtn.disabled=false;sbtn.textContent=ru?'✨ Удиви меня':'✨ Surprise Me';
      if(x.name){clearActive();lastAiName=x.name.substring(0,15);snm.style.color='#fbbf24';snm.textContent=lastAiName+' ✨';}
      else{snm.style.color='#ef4444';snm.textContent=ru?'⚠ Ошибка AI':'⚠ AI error';setTimeout(function(){snm.textContent='';snm.style.color='#fbbf24';},4000);}
    }
  }
  function connect(){
    try{ws=new WebSocket('ws://'+location.hostname+':81/');}catch(e){wst=setTimeout(connect,wsDelay);wsDelay=Math.min(wsDelay*2,30000);return;}
    ws.onopen=function(){wsDelay=3000;pullFails=0;hideOffline();};
    ws.onmessage=function(e){try{applyState(JSON.parse(e.data));}catch(err){}};
    ws.onclose=function(){clearTimeout(wst);wst=setTimeout(connect,wsDelay);wsDelay=Math.min(wsDelay*2,30000);if(wsDelay>3000){pullFails++;if(pullFails>=3)showOffline();}};
    ws.onerror=function(){ws.close();};
  }
  connect();
})();
