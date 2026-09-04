import {createClient} from 'https://esm.sh/@supabase/supabase-js@2';

const cors={'Access-Control-Allow-Origin':'*','Access-Control-Allow-Headers':'apikey, content-type, x-enrollment-key','Access-Control-Allow-Methods':'POST, OPTIONS'};
const json=(body:unknown,status=200)=>new Response(JSON.stringify(body),{status,headers:{...cors,'Content-Type':'application/json'}});
const hex=(bytes:ArrayBuffer)=>[...new Uint8Array(bytes)].map(value=>value.toString(16).padStart(2,'0')).join('');

Deno.serve(async request=>{
  if(request.method==='OPTIONS')return new Response('ok',{headers:cors});
  if(request.method!=='POST')return json({error:'Método no permitido'},405);
  try{
    const expected=Deno.env.get('DEVICE_ENROLLMENT_KEY')||'';
    if(!expected||request.headers.get('x-enrollment-key')!==expected)return json({error:'Credencial de fabricación inválida'},401);
    const body=await request.json(),id=String(body.device_id||'').toUpperCase(),mac=String(body.mac||'').toUpperCase(),claimCode=String(body.claim_code||'');
    if(!/^ESP32-[0-9A-F]{4}$/.test(id)||!/^[0-9A-F:]{17}$/.test(mac)||!/^[A-Z0-9-]{10,32}$/.test(claimCode))return json({error:'Datos de dispositivo inválidos'},400);
    const service=createClient(Deno.env.get('SUPABASE_URL')!,Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!,{auth:{persistSession:false,autoRefreshToken:false}});
    const secretHash=hex(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(`${id}:${claimCode}`)));
    const {data:existing}=await service.from('device_inventory').select('id').eq('id',id).maybeSingle();
    const record={id,mac,firmware:String(body.firmware||''),last_seen:new Date().toISOString(),metadata:{ip:body.ip||'',chip_model:body.chip_model||'ESP32',flash_size:body.flash_size||0}};
    const query=existing?service.from('device_inventory').update(record).eq('id',id):service.from('device_inventory').insert({...record,claim_code:claimCode,secret_hash:secretHash});
    const {error}=await query;if(error)throw error;
    return json({ok:true,device_id:id,status:existing?'known':'registered'},existing?200:201);
  }catch(error){return json({error:error instanceof Error?error.message:String(error)},400)}
});
