sb.addEventListener('input',function(){pb(+sb.value);clearTimeout(t1);t1=setTimeout(function(){xf('/setb?v='+sb.value);},120);clearActive();});
sc.addEventListener('input',function(){pc(+sc.value);clearTimeout(t2);t2=setTimeout(function(){xf('/setc?v='+sc.value);},120);clearActive();});
sco.addEventListener('input',function(){pco(+sco.value);clearTimeout(t3);t3=setTimeout(function(){xf('/setco?v='+sco.value);},120);clearActive();});
ssp.addEventListener('input',function(){psp(+ssp.value);clearTimeout(t4);t4=setTimeout(function(){xf('/setsp?v='+ssp.value);},120);clearActive();});
sbl.addEventListener('input',function(){pbl(+sbl.value);clearTimeout(t5);t5=setTimeout(function(){xf('/setbl?v='+sbl.value);},120);clearActive();});
[0,1,2,3].forEach(function(t){document.getElementById('tb'+t).onclick=function(){xf('/settheme?v='+t).then(pull).catch(function(){});clearActive();};});
document.getElementById('rst').onclick=function(){xf('/reset').then(pull).catch(function(){});clearActive();};
