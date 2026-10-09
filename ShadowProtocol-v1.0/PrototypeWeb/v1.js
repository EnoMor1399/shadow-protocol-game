(() => {
'use strict';

const RELEASE='1.0.1';
const RELEASE_LABEL=`v${RELEASE} // STABILITY PATCH`;
const BUILD_ID=`SP-${RELEASE}`;
const $ = (s) => document.querySelector(s);
const timers = new Set();
let observer = null;
let pollingTimer = null;
let objectiveTimer = 0;

function later(fn,ms){
  const id=setTimeout(()=>{timers.delete(id);fn();},ms);
  timers.add(id);
  return id;
}
function clearManagedTimers(){timers.forEach(clearTimeout);timers.clear();}
function readStorage(key,fallback){
  try{
    const modern=localStorage.getItem(`sp.v101.${key}`);
    if(modern!==null)return modern;
    const legacy=localStorage.getItem(`sp.v10.${key}`);
    return legacy!==null?legacy:fallback;
  }catch{return fallback;}
}
function writeStorage(key,value){try{localStorage.setItem(`sp.v101.${key}`,value);}catch{/* private/restricted storage: keep in-memory state */}}

const state = {
  hud: readStorage('hud','FULL'),
  contrast: readStorage('contrast','STANDARD'),
  motion: readStorage('motion','FULL'),
  telemetry: readStorage('telemetry','ON'),
  hints: readStorage('hints','ON'),
  intelNotices: readStorage('intelNotices','ON'),
  settingsFromGame: false,
  deploymentTimer: 0,
  deploymentStepTimers: [],
  lastObjective: '',
  lastIntegrity: ''
};

function addStylesheet(){
  ['v1.css','v101.css'].forEach(href=>{
    if(document.querySelector(`link[href="${href}"]`)) return;
    const link=document.createElement('link');link.rel='stylesheet';link.href=href;document.head.appendChild(link);
  });
}

function settingCard(label,value,desc,id){
  return `<div class="settingCard"><div><small>${label}</small><b id="${id}Value">${value}</b></div><button id="${id}" class="settingToggle">CHANGE</button><p>${desc}</p></div>`;
}

function injectUI(){
  document.title=`SHADOW PROTOCOL — v${RELEASE} Embassy Vertical Slice`;
  const top=$('.topStatus');
  if(top){
    const build=top.querySelector('.buildStatus');if(build)build.textContent=RELEASE_LABEL;
    if(!$('#patchStatus')){const p=document.createElement('span');p.id='patchStatus';p.className='patchStatus';p.textContent='PATCH VERIFIED';top.appendChild(p);}
  }
  const footer=$('.menuFooter');if(footer&&footer.lastElementChild)footer.lastElementChild.textContent=`BUILD ${RELEASE} // EMBASSY VERTICAL SLICE`;
  const nav=$('.mainNav');
  if(nav&&!$('#settingsBtn')){
    const b=document.createElement('button');b.id='settingsBtn';b.innerHTML='<span class="navIndex">05</span><span>SETTINGS</span><small>DISPLAY // CONTROLS</small>';nav.appendChild(b);
  }

  const stage=$('#stage');if(!stage)return;
  if(!$('#settingsPanel')){
    const s=document.createElement('section');s.id='settingsPanel';s.className='panel settingsPanel';
    s.innerHTML=`<aside class="settingsRail"><div><div class="eyebrow">SYSTEM // LOCAL CLIENT</div><h3>OPERATOR SETTINGS</h3></div><p>Configure presentation and information density without changing competitive authority or server-owned game rules.</p><div class="versionCard"><small>ACTIVE BUILD</small><b>${RELEASE_LABEL}</b><span>${BUILD_ID} // EMBASSY</span></div><div id="clientDiagnostic" class="clientDiagnostic"><small>CLIENT INTEGRITY</small><b>CHECKING</b><span>Validating critical interface anchors.</span></div></aside><div class="settingsBody"><div class="panelHeader"><div><div class="eyebrow">PRESENTATION // ACCESSIBILITY</div><h2>SETTINGS & KEYBINDS</h2></div><div class="stepCounter"><b>SP</b><span>/ 10</span></div></div><div class="settingsGrid">${settingCard('HUD DENSITY',state.hud,'Full preserves all tactical telemetry. Minimal keeps objectives, score, health and weapon data dominant.','hudSetting')}${settingCard('CONTRAST',state.contrast,'Standard uses the classified-command palette. Enhanced increases panel separation and text contrast.','contrastSetting')}${settingCard('MOTION PROFILE',state.motion,'Reduced motion suppresses non-essential interface animation while preserving gameplay feedback.','motionSetting')}${settingCard('NETWORK TELEMETRY',state.telemetry,'Show or hide the local training telemetry strip. Competitive server authority is unchanged.','telemetrySetting')}${settingCard('INPUT HINTS',state.hints,'Show or hide the bottom-left quick-key strip while keeping interaction prompts available.','hintsSetting')}${settingCard('INTEL NOTICES',state.intelNotices,'Control objective-site confirmation notices without hiding the primary objective card.','intelSetting')}</div><div class="keybindPanel"><h4>TACTICAL KEYBINDS // v${RELEASE}</h4><div class="keybindGrid"><span>MOVE <kbd>WASD</kbd></span><span>AIM <kbd>RMB</kbd></span><span>FIRE <kbd>LMB</kbd></span><span>RELOAD <kbd>R</kbd></span><span>INTERACT <kbd>E</kbd></span><span>HACK <kbd>H</kbd></span><span>BREACH <kbd>B</kbd></span><span>SPRINT <kbd>SHIFT</kbd></span><span>CROUCH <kbd>CTRL</kbd></span><span>LEAN <kbd>Z / C</kbd></span><span>VAULT <kbd>SPACE</kbd></span><span>TACTICAL MAP <kbd>TAB</kbd></span><span>GADGET <kbd>Q</kbd></span><span>THROWABLE <kbd>G / F</kbd></span><span>SQUAD ORDER <kbd>V</kbd></span><span>FORTIFY <kbd>X</kbd></span><span>OPTIC <kbd>Y</kbd></span><span>INSPECT <kbd>I</kbd></span><span>HUD MODE <kbd>F1</kbd></span><span>SETTINGS <kbd>ESC</kbd></span></div></div><div class="settingsActions"><button id="settingsClose" class="primary">RETURN <span>→</span></button></div></div>`;
    stage.appendChild(s);
  }
  if(!$('#siteRibbon')){const d=document.createElement('div');d.id='siteRibbon';d.className='siteRibbon';d.innerHTML='<small>INTELLIGENCE CONFIRMATION</small><b>OBJECTIVE SITE</b><span>AWAITING VERIFIED SIGNAL</span>';stage.appendChild(d);}
  if(!$('#coverBadge')){const d=document.createElement('div');d.id='coverBadge';d.className='coverBadge';d.innerHTML='<i></i><span>COVER AVAILABLE // Z/C TO PEEK</span>';stage.appendChild(d);}
  if(!$('#deploymentBriefing')){const d=document.createElement('div');d.id='deploymentBriefing';d.className='deploymentBriefing hidden';d.innerHTML='<div class="deploymentCore"><small>DIRECTORATE NINE // TASK FORCE SPECTRE</small><h2>OPERATION <span>FIRST CONTACT</span></h2><span id="deploymentMeta">EMBASSY // PROTOCOL</span><div class="deploymentSteps"><div>01 // AUTH</div><div>02 // LOADOUT</div><div>03 // INSERTION</div><div>04 // LIVE</div></div></div>';stage.appendChild(d);}
  if(!$('.verticalSliceTag')){const d=document.createElement('div');d.className='verticalSliceTag';d.innerHTML=`<i></i> ${RELEASE_LABEL}`;stage.appendChild(d);}
  else $('.verticalSliceTag').innerHTML=`<i></i> ${RELEASE_LABEL}`;
  if(!$('.settingsHint')){const d=document.createElement('div');d.className='settingsHint';d.innerHTML='<kbd>F1</kbd> HUD DENSITY // <kbd>ESC</kbd> SETTINGS';stage.appendChild(d);}

  const readyMeta=$('.readyMeta');
  if(readyMeta&&!$('#buildIntegrityState')){
    const d=document.createElement('div');d.id='buildIntegrityState';d.className='networkState connected buildIntegrityState';d.innerHTML=`<small>CLIENT BUILD</small><b>${BUILD_ID} // CHECKING</b>`;readyMeta.appendChild(d);
  }
  const hud=$('#hud');
  if(hud&&!$('#sessionIntegrity')){
    const d=document.createElement('div');d.id='sessionIntegrity';d.className='sessionIntegrity';d.innerHTML=`<i></i><span>SECURE SESSION // ${BUILD_ID}</span>`;hud.appendChild(d);
  }
}

function apply(){
  document.body.classList.toggle('hud-minimal',state.hud==='MINIMAL');
  document.body.classList.toggle('v10-contrast',state.contrast==='ENHANCED');
  document.body.classList.toggle('v10-reduced-motion',state.motion==='REDUCED');
  document.body.classList.toggle('v101-no-hints',state.hints==='OFF');
  document.body.classList.toggle('v101-no-intel-notices',state.intelNotices==='OFF');
  const telemetry=$('#netTelemetry');if(telemetry)telemetry.style.display=state.telemetry==='ON'?'flex':'none';
  const quickKeys=$('.quickKeys');if(quickKeys)quickKeys.setAttribute('aria-hidden',state.hints==='OFF'?'true':'false');
  if(state.intelNotices==='OFF')$('#siteRibbon')?.classList.remove('show');
  const set=(id,val)=>{const el=$(`#${id}Value`);if(el)el.textContent=val;};
  set('hudSetting',state.hud);set('contrastSetting',state.contrast);set('motionSetting',state.motion);set('telemetrySetting',state.telemetry);set('hintsSetting',state.hints);set('intelSetting',state.intelNotices);
}
function save(){
  ['hud','contrast','motion','telemetry','hints','intelNotices'].forEach(k=>writeStorage(k,state[k]));
  apply();
}
function cycle(key,vals){const i=Math.max(0,vals.indexOf(state[key]));state[key]=vals[(i+1)%vals.length];save();}

function panelsOff(){document.querySelectorAll('.panel').forEach(p=>p.classList.remove('show'));}
function isLiveOperation(){return !$('#hud')?.classList.contains('hidden');}
function openSettings(){
  state.settingsFromGame=isLiveOperation();
  panelsOff();$('#settingsPanel')?.classList.add('show');document.exitPointerLock?.();
}
function closeSettings(){
  $('#settingsPanel')?.classList.remove('show');
  if(state.settingsFromGame){$('#resumeOverlay')?.classList.remove('hidden');}
  else $('#menu')?.classList.add('show');
}

function bind(){
  $('#settingsBtn')?.addEventListener('click',openSettings);
  $('#settingsClose')?.addEventListener('click',closeSettings);
  $('#hudSetting')?.addEventListener('click',()=>cycle('hud',['FULL','MINIMAL']));
  $('#contrastSetting')?.addEventListener('click',()=>cycle('contrast',['STANDARD','ENHANCED']));
  $('#motionSetting')?.addEventListener('click',()=>cycle('motion',['FULL','REDUCED']));
  $('#telemetrySetting')?.addEventListener('click',()=>cycle('telemetry',['ON','OFF']));
  $('#hintsSetting')?.addEventListener('click',()=>cycle('hints',['ON','OFF']));
  $('#intelSetting')?.addEventListener('click',()=>cycle('intelNotices',['ON','OFF']));
  addEventListener('keydown',e=>{
    if(e.repeat)return;
    if(e.code==='F1'){e.preventDefault();cycle('hud',['FULL','MINIMAL']);return;}
    if(e.code==='Escape'){
      if($('#settingsPanel')?.classList.contains('show')){e.preventDefault();e.stopImmediatePropagation();closeSettings();return;}
      e.preventDefault();openSettings();
    }
  },true);
  $('#deployBtn')?.addEventListener('click',showDeployment);
  $('#nextRoundBtn')?.addEventListener('click',showDeployment);
  document.addEventListener('visibilitychange',()=>{
    if(document.hidden&&isLiveOperation())document.exitPointerLock?.();
    if(!document.hidden&&isLiveOperation()&&document.pointerLockElement!==$('#game'))$('#resumeOverlay')?.classList.remove('hidden');
  });
  addEventListener('beforeunload',cleanup,{once:true});
}

function showDeployment(){
  const overlay=$('#deploymentBriefing');if(!overlay)return;
  if(state.deploymentTimer)clearTimeout(state.deploymentTimer);
  state.deploymentStepTimers.forEach(clearTimeout);state.deploymentStepTimers=[];
  const mode=$('#roundMode')?.textContent||'PROTOCOL';const spawn=(mode.split('//').pop()||'ALPHA').trim();
  $('#deploymentMeta').textContent=`EMBASSY // ${mode.replace(/\s+/g,' ')} // ${spawn} // ${BUILD_ID}`;
  overlay.classList.remove('hidden','fade');const steps=[...overlay.querySelectorAll('.deploymentSteps div')];steps.forEach(x=>x.classList.remove('active'));
  steps.forEach((x,i)=>state.deploymentStepTimers.push(later(()=>x.classList.add('active'),220+i*360)));
  state.deploymentTimer=later(()=>{overlay.classList.add('fade');later(()=>overlay.classList.add('hidden'),460);},2300);
}

function objectiveRibbon(){
  const text=$('#objective')?.textContent||'';if(text===state.lastObjective)return;state.lastObjective=text;
  const m=text.match(/SECURE SP DATA\s*\/\/\s*([A-Z0-9-]+)/i);if(!m||state.intelNotices==='OFF')return;
  const ribbon=$('#siteRibbon');if(!ribbon)return;const site=m[1].toUpperCase();const names={'ARCHIVE-A':'ARCHIVE CORE','VAULT-B':'SECURITY VAULT','SAFE-C':'DIPLOMATIC SAFE'};
  ribbon.querySelector('b').textContent=`${site} // ${names[site]||'CONFIRMED SITE'}`;ribbon.querySelector('span').textContent='VERIFIED SIGINT // RECOVER ENCRYPTED PACKAGE';
  if(objectiveTimer)clearTimeout(objectiveTimer);ribbon.classList.remove('show');void ribbon.offsetWidth;ribbon.classList.add('show');objectiveTimer=later(()=>ribbon.classList.remove('show'),4200);
}
function coverStatus(){
  const prompt=$('#prompt')?.textContent||'',weapon=$('#weaponState')?.textContent||'',badge=$('#coverBadge');if(!badge)return;
  const available=/VAULT LOW COVER|CRACK ENTRY|OPEN FULLY/i.test(prompt)||/LEAN/i.test(weapon);const peek=/LEAN/i.test(weapon);
  badge.classList.toggle('show',available);badge.classList.toggle('peek',peek);const text=badge.querySelector('span');if(text)text.textContent=peek?'PEEK ACTIVE // MINIMIZE EXPOSURE':'COVER / ENTRY ANGLE // Z/C TO PEEK';
}

function integritySnapshot(){
  const required=['game','menu','rolePanel','readyRoom','planning','hud','objective','roundState','roundMode','netTelemetry','resumeOverlay'];
  const missing=required.filter(id=>!document.getElementById(id));
  const network=$('#networkState')?.textContent||'';
  const auth=$('#authState')?.textContent||'';
  const reconnect=!$('#reconnectBanner')?.classList.contains('hidden');
  const good=missing.length===0;
  const sessionOk=/CONNECTED/i.test(network)&&/VERIFIED/i.test(auth)&&!reconnect;
  return {good,sessionOk,reconnect,missing};
}
function refreshIntegrity(){
  const snap=integritySnapshot();const signature=`${snap.good}:${snap.sessionOk}:${snap.reconnect}:${snap.missing.join(',')}`;if(signature===state.lastIntegrity)return;state.lastIntegrity=signature;
  const ready=$('#buildIntegrityState');
  if(ready){
    ready.classList.toggle('connected',snap.good);ready.classList.toggle('degraded',!snap.good);
    const b=ready.querySelector('b');if(b)b.textContent=snap.good?`${BUILD_ID} // VERIFIED`:`${BUILD_ID} // DEGRADED`;
  }
  const diagnostic=$('#clientDiagnostic');
  if(diagnostic){const b=diagnostic.querySelector('b'),s=diagnostic.querySelector('span');diagnostic.classList.toggle('ok',snap.good);diagnostic.classList.toggle('bad',!snap.good);if(b)b.textContent=snap.good?'VERIFIED':'DEGRADED';if(s)s.textContent=snap.good?'Critical interface anchors present.':'Missing: '+snap.missing.join(', ');}
  const chip=$('#sessionIntegrity');
  if(chip){
    chip.classList.toggle('warning',snap.reconnect||!snap.sessionOk);chip.classList.toggle('fault',!snap.good);
    const text=chip.querySelector('span');
    if(text)text.textContent=!snap.good?`CLIENT DEGRADED // ${BUILD_ID}`:snap.reconnect?'RECONNECT WINDOW // SLOT RESERVED':snap.sessionOk?`SECURE SESSION // ${BUILD_ID}`:`SESSION CHECK // ${BUILD_ID}`;
  }
  const patch=$('#patchStatus');if(patch){patch.classList.toggle('warning',!snap.good);patch.textContent=snap.good?'PATCH VERIFIED':'PATCH DEGRADED';}
}

function observe(){
  const targets=['objective','prompt','weaponState','roundState','roundMode','networkState','authState','reconnectBanner'].map(id=>$(`#${id}`)).filter(Boolean);
  observer=new MutationObserver(()=>{objectiveRibbon();coverStatus();refreshIntegrity();});
  targets.forEach(t=>observer.observe(t,{subtree:true,childList:true,characterData:true,attributes:true,attributeFilter:['class']}));
  pollingTimer=setInterval(()=>{objectiveRibbon();coverStatus();refreshIntegrity();},750);
}
function cleanup(){
  observer?.disconnect();observer=null;
  if(pollingTimer)clearInterval(pollingTimer);pollingTimer=null;
  if(objectiveTimer)clearTimeout(objectiveTimer);objectiveTimer=0;
  clearManagedTimers();
}

addStylesheet();injectUI();apply();refreshIntegrity();bind();observe();
})();
