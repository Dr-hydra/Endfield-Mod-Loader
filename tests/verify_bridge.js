'use strict';
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict'),path=require('node:path');
const root=path.resolve(__dirname,'..');
const windows=fs.readFileSync(path.join(root,'src/EModLoader.Core/WebBridge.cs'),'utf8').match(/Script = """\r?\n([\s\S]*?)\r?\n    """;/)[1].split(/\r?\n/).map(line=>line.slice(4)).join('\n');
const android=fs.readFileSync(path.join(root,'src/android/app/src/main/assets/module-bridge.js'),'utf8');
async function check(script,platform){
  const sent=[],timers=new Map();let sequence=0;const window={};window.top=window;
  const context={window,console,setTimeout:fn=>{timers.set(++sequence,fn);return sequence;},clearTimeout:id=>timers.delete(id)};
  if(platform==='windows')window.chrome={webview:{postMessage:message=>sent.push(message),addEventListener:()=>{}}};
  else window.BetterEndfieldModuleHost={postMessage:message=>sent.push(JSON.parse(message))};
  vm.runInNewContext(script,context);const api=window.betterEndfield;assert.ok(api);assert.equal(window.betterEndfieldNext,undefined);
  let promise=api.readConfig(),request=sent.pop();assert.equal(request.protocol,'better-endfield.module-ui.v1');
  window.__beModuleDeliver({kind:'bridge_reply',request_id:request.request_id,value:{oldField:'preserved'}});assert.equal((await promise).oldField,'preserved');
  promise=api.send({module_id:'other',value:[1,2]});request=sent.pop();assert.equal(request.module_id,undefined);assert.equal(request.payload.module_id,'other');
  window.__beModuleDeliver({kind:'bridge_reply',request_id:request.request_id,value:{accepted:true}});assert.equal((await promise).accepted,true);
  const events=[];const unsubscribe=api.onmessage(message=>events.push(message));window.__beModuleDeliver({kind:'runtime_message',message:{kind:'event',body:{x:1}}});assert.equal(events.length,1);unsubscribe();
  window.__beModuleDeliver({kind:'runtime_message',message:{kind:'event'}});assert.equal(events.length,1);
  promise=api.status();request=sent.pop();window.__beModuleDeliver({kind:'bridge_reply',request_id:request.request_id,error:'offline'});await assert.rejects(promise,/offline/);
  promise=api.status();timers.values().next().value();await assert.rejects(promise,/timed out/);
  console.log(`PASS ${platform}: original creator API, identity boundary, replies/events, unsubscribe, errors and timeout`);
}
(async()=>{await check(windows,'windows');await check(android,'android');})().catch(error=>{console.error(error);process.exitCode=1;});
