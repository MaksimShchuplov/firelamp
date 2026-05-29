pull();var wsLive=false,pollTid;
(function(){
  var ws,wst,wsDelay=3000;
  pollTid=setInterval(function(){if(ws&&ws.readyState!==1)wsLive=false;if(!document.hidden&&!wsLive)pull();},5000);
  function connect(){
    try{ws=new WebSocket('ws://'+location.hostname+':81/');}catch(e){clearTimeout(wst);wst=setTimeout(connect,wsDelay);wsDelay=Math.min(wsDelay*2,30000);return;}
    ws.onopen=function(){wsLive=true;wsDelay=3000;pullFails=0;hideOffline();};
    ws.onmessage=function(e){try{applyState(JSON.parse(e.data));}catch(err){}};
    ws.onclose=function(){wsLive=false;clearTimeout(wst);if(wsDelay>3000){pullFails++;if(pullFails>=3)showOffline();}wst=setTimeout(connect,wsDelay);wsDelay=Math.min(wsDelay*2,30000);};
    ws.onerror=function(){if(!wst)wst=setTimeout(connect,wsDelay);};
  }
  connect();
  document.addEventListener('visibilitychange',function(){
    if(!document.hidden&&!wsLive){clearTimeout(wst);wst=null;wsDelay=3000;connect();}
  });
})();
