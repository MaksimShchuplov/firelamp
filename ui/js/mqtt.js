document.getElementById('mqsave').onclick=function(){
  var ip=document.getElementById('mqip').value.trim();
  var pt=document.getElementById('mqpt').value.trim();
  var u=document.getElementById('mqu').value.trim();
  var p=document.getElementById('mqp').value.trim();
  var t=document.getElementById('mqt').value.trim()||'firelamp';
  var btn=document.getElementById('mqsave'), st=document.getElementById('mqst');
  btn.disabled=true;
  var body='ip='+encodeURIComponent(ip)+'&pt='+encodeURIComponent(pt)+
           '&u='+encodeURIComponent(u)+'&p='+encodeURIComponent(p)+
           '&t='+encodeURIComponent(t);
  
  fetch('/setmqtt',{method:'POST',headers:{'X-Requested-With':'firelamp','Content-Type':'application/x-www-form-urlencoded'},body:body})
  .then(r=>r.json()).then(function(x){
    if(x.ok){st.textContent=ru?'Сохранено ✓':'Saved ✓';st.style.color='#4ade80';}
    else{st.textContent=ru?'Ошибка':'Error';st.style.color='#f87171';}
    setTimeout(function(){st.textContent='';btn.disabled=false;},1500);
  }).catch(function(){
    st.textContent=ru?'Ошибка сети':'Network Error';st.style.color='#f87171';
    setTimeout(function(){st.textContent='';btn.disabled=false;},1500);
  });
};

function loadMqtt(){
  fetch('/getmqtt').then(r=>r.json()).then(function(x){
    if(x.ip) document.getElementById('mqip').value=x.ip;
    if(x.pt) document.getElementById('mqpt').value=x.pt;
    if(x.u) document.getElementById('mqu').value=x.u;
    // Password is write-only on the server; show a placeholder when one is saved.
    var pf=document.getElementById('mqp');
    pf.placeholder=x.p_set?(ru?'Сохранён (введите для замены)':'Saved (enter to replace)'):'';
    if(x.t) document.getElementById('mqt').value=x.t;
  }).catch(()=>{});
}
// Call when modal opens or on load. Since modal opens on ibtn, we can just load it once on boot.
setTimeout(loadMqtt, 500);
