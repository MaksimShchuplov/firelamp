pull();var pollTid=setInterval(function(){if(!document.hidden)pull()},5000);
(function(){
  var ws,wst,wsDelay=3000;
  function connect(){
    try{ws=new WebSocket('ws://'+location.hostname+':81/');}catch(e){wst=setTimeout(connect,wsDelay);wsDelay=Math.min(wsDelay*2,30000);return;}
    ws.onopen=function(){wsDelay=3000;pullFails=0;hideOffline();};
    ws.onmessage=function(e){try{applyState(JSON.parse(e.data));}catch(err){}};
    ws.onclose=function(){clearTimeout(wst);wst=setTimeout(connect,wsDelay);wsDelay=Math.min(wsDelay*2,30000);if(wsDelay>3000){pullFails++;if(pullFails>=3)showOffline();}};
    ws.onerror=function(){ws.close();};
  }
  connect();
})();
