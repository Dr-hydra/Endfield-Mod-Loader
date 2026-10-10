'use strict';
const bridge=window.endfieldModLoader||window.betterEndfield;
const byId=id=>document.getElementById(id);
const flags=['enabled','active','hide_head','fill_neck','diagnostics'];
const numbers=['fov','neck_range','look_limit','turn_speed','toggle_key'];
let configuration={},ready=false,inFlight=false;
const phases={waiting_runtime:'等待游戏运行时',ready:'游戏接口已就绪',contract_mismatch:'游戏接口不匹配',hook_failed:'相机挂接失败',failed:'初始化失败',stopped:'模块已停用'};
function feedback(text){byId('feedback').textContent=text;}
function render(state){
  byId('runtime').textContent=JSON.stringify(state,null,2);
  ready=state.phase==='ready'&&state.enabled&&!state.cleanup_pending;
  byId('toggle').disabled=byId('exit').disabled=!ready;
  byId('phase').textContent=state.active?'第一人称已开启':(phases[state.phase]||state.phase||'等待状态');
  byId('detail').textContent=state.cleanup_pending?'等待游戏线程恢复相机与模型。':state.detail||
    (state.phase==='waiting_runtime'?'离线模式不会开启游戏效果。':'');
}
async function command(action){
  if(!bridge)throw new Error('请从 E Mod Loader 打开此页面。');
  const accepted=await bridge.send({action});
  if(accepted.accepted===false)throw new Error('宿主未接受命令。');
}
async function refresh(){
  if(inFlight)return;
  inFlight=true;
  try{
    if(!bridge)throw new Error('请从 E Mod Loader 打开此页面。');
    const status=await bridge.status();
    byId('connection').textContent=!status.connected?'宿主未连接':status.mode==='game'?'游戏宿主已连接':'离线宿主已连接';
    if(status.connected)await command('status');
    else{ready=false;byId('toggle').disabled=byId('exit').disabled=true;}
  }catch(error){ready=false;byId('toggle').disabled=byId('exit').disabled=true;feedback(error.message);}
  finally{inFlight=false;}
}
if(bridge)bridge.onmessage(message=>{
  if(message.kind==='reply'&&message.body?.phase){render(message.body);if(message.result!==0)feedback('当前游戏状态无法执行此操作。');}
});
byId('settings').onsubmit=async event=>{
  event.preventDefault();
  try{
    const value={...configuration};
    for(const id of flags)value[id]=byId(id).checked;
    for(const id of numbers)value[id]=Number(byId(id).value);
    const result=await bridge.saveConfig(value);
    if(result?.accepted===false||result?.result>0)throw new Error('模块拒绝了配置。');
    configuration=value;feedback('配置已保存。游戏效果将在游戏线程处理。');await refresh();
  }catch(error){feedback(error.message);}
};
for(const action of ['toggle','exit'])byId(action).onclick=async()=>{
  try{await command(action);feedback('请求已提交，等待游戏帧。');await refresh();}catch(error){feedback(error.message);}
};
byId('refresh').onclick=refresh;
byId('toggle').disabled=byId('exit').disabled=true;
(async()=>{
  try{
    if(!bridge)throw new Error('请从 E Mod Loader 打开此页面。');
    configuration=await bridge.readConfig();
    const defaults={enabled:true,active:false,hide_head:true,fill_neck:true,diagnostics:false,fov:75,neck_range:1,look_limit:90,turn_speed:360,toggle_key:189};
    for(const id of flags)byId(id).checked=configuration[id]??defaults[id];
    for(const id of numbers){
      const value=configuration[id]??defaults[id];
      if(id==='toggle_key'&&![...byId(id).options].some(option=>Number(option.value)===value)){
        const option=document.createElement('option');option.value=String(value);option.textContent='自定义 VK '+value;byId(id).appendChild(option);
      }
      byId(id).value=String(value);
    }
    await refresh();
  }catch(error){feedback(error.message);}
})();
const poll=setInterval(()=>{if(!document.hidden)refresh();},2000);
window.addEventListener('pagehide',()=>clearInterval(poll),{once:true});
