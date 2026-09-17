(() => {
'use strict';

const $ = (s) => document.querySelector(s);
const state = {
  hud: localStorage.getItem('sp.v10.hud') || 'FULL',
  contrast: localStorage.getItem('sp.v10.contrast') || 'STANDARD',
  motion: localStorage.getItem('sp.v10.motion') || 'FULL',
  telemetry: localStorage.getItem('sp.v10.telemetry') || 'ON',
  settingsFromGame: false,
  deploymentTimer: 0,
  lastObjective: ''
};

function addStylesheet(){
  if(document.querySelector('link[href="v1.css"]')) return;
  const link=document.createElement('link');link.rel='stylesheet';link.href='v1.css';document.head.appendChild(link);
}

function settingCard(label,value,desc,id){
  return `<div class="settingCard"><div><small>${label}</small><b id="${id}Value">${value}</b></div><button id="${id}" class="settingToggle">CHANGE</button><p>${desc}</p></div>`;
}

function injectUI(){
  const top=$('.topStatus');
  if(top){const build=top.querySelector('.buildStatus');if(build)build.textContent='v1.0 // VERTICAL SLICE';}
  const footer=$('.menuFooter');if(footer&&footer.lastElementChild)footer.lastElementChild.textContent='BUILD 1.0 // EMBASSY VERTICAL SLICE';
  const nav=$('.mainNav');
  if(nav&&!$('#settingsBtn')){
    const b=document.createElement('button');b.id='settingsBtn';b.innerHTML='<span class="navIndex">05</span><span>SETTINGS</span><small>DISPLAY // CONTROLS</small>';nav.appendChild(b);
  }

  const stage=$('#stage');if(!stage)return;
  if(!$('#settingsPanel')){
    const s=document.createElement('section');s.id='settingsPanel';s.className='panel settingsPanel';
    s.innerHTML=`<aside class="settingsRail"><div><div class="eyebrow">SYSTEM // LOCAL CLIENT</div><h3>OPERATOR SETTINGS</h3></div><p>Configure presentation and information density without changing competitive authority or server-owned game rules.</p><div class="versionCard"><small>ACTIVE BUILD</small><b>v1.0 // EMBASSY VERTICAL SLICE</b></div></aside><div class="settingsBody"><div class="panelHeader"><div><div class="eyebrow">PRESENTATION // ACCESSIBILITY</div><h2>SETTINGS & KEYBINDS</h2></div><div class="stepCounter"><b>SP</b><span>/ 10</span></div></div><div class="settingsGrid">${settingCard('HUD DENSITY',state.hud,'Full preserves all tactical telemetry. Minimal keeps objectives, score, health and weapon data dominant.','hudSetting')}${settingCard('CONTRAST',state.contrast,'Standard uses the classified-command palette. Enhanced increases panel separation and text contrast.','contrastSetting')}${settingCard('MOTION PROFILE',state.motion,'Reduced motion suppresses non-essential interface animation while preserving gameplay feedback.','motionSetting')}${settingCard('NETWORK TELEMETRY',state.telemetry,'Show or hide the local training telemetry strip. Competitive server authority is unchanged.','telemetrySetting')}</div><div class="keybindPanel"><h4>TACTICAL KEYBINDS // v1.0</h4><div class="keybindGrid"><span>MOVE <kbd>WASD</kbd></span><span>AIM <kbd>RMB</kbd></span><span>FIRE <kbd>LMB</kbd></span><span>RELOAD <kbd>R</kbd></span><span>INTERACT <kbd>E</kbd></span><span>HACK <kbd>H</kbd></span><span>BREACH <kbd>B</kbd></span><span>SPRINT <kbd>SHIFT</kbd></span><span>CROUCH <kbd>CTRL</kbd></span><span>LEAN <kbd>Z / C</kbd></span><span>VAULT <kbd>SPACE</kbd></span><span>TACTICAL MAP <kbd>TAB</kbd></span><span>GADGET <kbd>Q</kbd></span><span>THROWABLE <kbd>G / F</kbd></span><span>SQUAD ORDER <kbd>V</kbd></span><span>FORTIFY <kbd>X</kbd></span><span>OPTIC <kbd>Y</kbd></span><span>INSPECT <kbd>I</kbd></span><span>HUD MODE <kbd>F1</kbd></span><span>SETTINGS <kbd>ESC</kbd></span></div></div><div class="settingsActions"><button id="settingsClose" class="primary">RETURN <span>→</span></button></div></div>`;
    stage.appendChild(s);
  }
  if(!$('#siteRibbon')){const d=document.createElement('div');d.id='siteRibbon';d.className='siteRibbon';d.innerHTML='<small>INTELLIGENCE CONFIRMATION</small><b>OBJECTIVE SITE</b><span>AWAITING VERIFIED SIGNAL</span>';stage.appendChild(d);}
  if(!$('#coverBadge')){const d=document.createElement('div');d.id='coverBadge';d.className='coverBadge';d.innerHTML='<i></i><span>COVER AVAILABLE // Z/C TO PEEK</span>';stage.appendChild(d);}
  if(!$('#deploymentBriefing')){const d=document.createElement('div');d.id='deploymentBriefing';d.className='deploymentBriefing hidden';d.innerHTML='<div class="deploymentCore"><small>DIRECTORATE NINE // TASK FORCE SPECTRE</small><h2>OPERATION <span>FIRST CONTACT</span></h2><span id="deploymentMeta">EMBASSY // PROTOCOL</span><div class="deploymentSteps"><div>01 // AUTH</div><div>02 // LOADOUT</div><div>03 // INSERTION</div><div>04 // LIVE</div></div></div>';stage.appendChild(d);}
  if(!$('.verticalSliceTag')){const d=document.createElement('div');d.className='verticalSliceTag';d.innerHTML='<i></i> VERTICAL SLICE // PRODUCTION UX';stage.appendChild(d);}
  if(!$('.settingsHint')){const d=document.createElement('div');d.className='settingsHint';d.innerHTML='<kbd>F1</kbd> HUD DENSITY // <kbd>ESC</kbd> SETTINGS';stage.appendChild(d);}
}

function apply(){
  document.body.classList.toggle('hud-minimal',state.hud==='MINIMAL');
  document.body.classList.toggle('v10-contrast',state.contrast==='ENHANCED');
  document.body.classList.toggle('v10-reduced-motion',state.motion==='REDUCED');
  const telemetry=$('#netTelemetry');if(telemetry)telemetry.style.display=state.telemetry==='ON'?'flex':'none';
  const set=(id,val)=>{const el=$(`#${id}Value`);if(el)el.textContent=val;};set('hudSetting',state.hud);set('contrastSetting',state.contrast);set('motionSetting',state.motion);set('telemetrySetting',state.telemetry);
}
function save(){localStorage.setItem('sp.v10.hud',state.hud);localStorage.setItem('sp.v10.contrast',state.contrast);localStorage.setItem('sp.v10.motion',state.motion);localStorage.setItem('sp.v10.telemetry',state.telemetry);apply();}
function cycle(key,vals){const i=vals.indexOf(state[key]);state[key]=vals[(i+1)%vals.length];save();}

function panelsOff(){document.querySelectorAll('.panel').forEach(p=>p.classList.remove('show'));}
function openSettings(){
  state.settingsFromGame=!$('#hud')?.classList.contains('hidden');
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
  addEventListener('keydown',e=>{
    if(e.code==='F1'){e.preventDefault();cycle('hud',['FULL','MINIMAL']);}
    if(e.code==='Escape'&&!$('#settingsPanel')?.classList.contains('show')){e.preventDefault();openSettings();}
  });
  $('#deployBtn')?.addEventListener('click',()=>showDeployment());
  $('#nextRoundBtn')?.addEventListener('click',()=>showDeployment());
}

function showDeployment(){
  const overlay=$('#deploymentBriefing');if(!overlay)return;
  clearTimeout(state.deploymentTimer);const mode=$('#roundMode')?.textContent||'PROTOCOL';const spawn=(mode.split('//').pop()||'ALPHA').trim();
  $('#deploymentMeta').textContent=`EMBASSY // ${mode.replace(/\s+/g,' ')} // ${spawn}`;
  overlay.classList.remove('hidden','fade');const steps=[...overlay.querySelectorAll('.deploymentSteps div')];steps.forEach(x=>x.classList.remove('active'));
  steps.forEach((x,i)=>setTimeout(()=>x.classList.add('active'),220+i*360));
  state.deploymentTimer=setTimeout(()=>{overlay.classList.add('fade');setTimeout(()=>overlay.classList.add('hidden'),460);},2300);
}

function objectiveRibbon(){
  const text=$('#objective')?.textContent||'';if(text===state.lastObjective)return;state.lastObjective=text;
  const m=text.match(/SECURE SP DATA\s*\/\/\s*([A-Z0-9-]+)/i);if(!m)return;
  const ribbon=$('#siteRibbon');if(!ribbon)return;const site=m[1].toUpperCase();const names={'ARCHIVE-A':'ARCHIVE CORE','VAULT-B':'SECURITY VAULT','SAFE-C':'DIPLOMATIC SAFE'};
  ribbon.querySelector('b').textContent=`${site} // ${names[site]||'CONFIRMED SITE'}`;ribbon.querySelector('span').textContent='VERIFIED SIGINT // RECOVER ENCRYPTED PACKAGE';ribbon.classList.remove('show');void ribbon.offsetWidth;ribbon.classList.add('show');setTimeout(()=>ribbon.classList.remove('show'),4200);
}
function coverStatus(){
  const prompt=$('#prompt')?.textContent||'',weapon=$('#weaponState')?.textContent||'',badge=$('#coverBadge');if(!badge)return;
  const available=/VAULT LOW COVER|CRACK ENTRY|OPEN FULLY/i.test(prompt)||/LEAN/i.test(weapon);const peek=/LEAN/i.test(weapon);
  badge.classList.toggle('show',available);badge.classList.toggle('peek',peek);badge.querySelector('span').textContent=peek?'PEEK ACTIVE // MINIMIZE EXPOSURE':'COVER / ENTRY ANGLE // Z/C TO PEEK';
}
function observe(){
  const targets=['objective','prompt','weaponState','roundState','roundMode'].map(id=>$(`#${id}`)).filter(Boolean);const mo=new MutationObserver(()=>{objectiveRibbon();coverStatus();});targets.forEach(t=>mo.observe(t,{subtree:true,childList:true,characterData:true}));
  setInterval(()=>{objectiveRibbon();coverStatus();},450);
}

addStylesheet();injectUI();apply();bind();observe();
})();
