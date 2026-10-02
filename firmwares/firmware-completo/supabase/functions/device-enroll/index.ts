import {createClient} from 'https://esm.sh/@supabase/supabase-js@2';

const cors={'Access-Control-Allow-Origin':'*','Access-Control-Allow-Headers':'apikey, content-type, x-enrollment-key','Access-Control-Allow-Methods':'POST, OPTIONS'};
const json=(body:unknown,status=200)=>new Response(JSON.stringify(body),{status,headers:{...cors,'Content-Type':'application/json'}});
const hex=(bytes:ArrayBuffer)=>[...new Uint8Array(bytes)].map(value=>value.toString(16).padStart(2,'0')).join('');

Deno.serve(async request=>{
  if(request.method==='OPTIONS')return new Response('ok',{headers:cors});
  if(request.method!=='POST')return json({error:'Método no permitido'},405);
  try{
    // KEY2 es la credencial activa. La anterior sólo queda como transición opcional.
    const expected=Deno.env.get('DEVICE_ENROLLMENT_KEY2')||Deno.env.get('DEVICE_ENROLLMENT_KEY')||'';
    if(!expected)return json({error:'DEVICE_ENROLLMENT_KEY2 no configurada en Supabase Secrets',code:'ENROLLMENT_NOT_CONFIGURED'},503);
    if(request.headers.get('x-enrollment-key')!==expected)return json({error:'Credencial de fabricación inválida',code:'ENROLLMENT_KEY_MISMATCH'},401);
    const body=await request.json(),id=String(body.device_id||'').toUpperCase(),mac=String(body.mac||'').toUpperCase(),claimCode=String(body.claim_code||'');
    if(!/^ESP32-[0-9A-F]{4}$/.test(id)||!/^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(mac)||!/^[A-Z0-9-]{10,32}$/.test(claimCode))return json({error:'Datos de dispositivo inválidos'},400);
    const service=createClient(Deno.env.get('SUPABASE_URL')!,Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!,{auth:{persistSession:false,autoRefreshToken:false}});
    const secretHash=hex(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(`${id}:${claimCode}`)));
    const {data:existing,error:lookupError}=await service.from('device_inventory').select('id,mac,secret_hash,status').eq('id',id).maybeSingle();
    if(lookupError)throw lookupError;
    if(existing&&(existing.mac!==mac||existing.secret_hash!==secretHash))return json({error:'ID existente con otra MAC o contraseña. No se sobrescribió.'},409);
    if(existing&&['blocked','retired'].includes(existing.status))return json({error:'Dispositivo bloqueado'},403);
    const serialNumber='ESP'+mac.replaceAll(':','').slice(-6);
    if(body.action==='telemetry'){
      if(!existing)return json({error:'Enrolar primero'},409);
      if(!/^[a-f0-9-]{3,40}$/.test(String(body.boot_id||''))||!Number.isSafeInteger(body.sequence)||body.sequence<0||!Number.isFinite(body.uptime_ms)||body.uptime_ms<0||body.uptime_ms>4294967295)return json({error:'Telemetría inválida'},400);
      for(const key of ['air_temperature','humidity','water_temperature','ph','tds_ppm','cpu_temperature'])if(body[key]!=null&&(!Number.isFinite(body[key])||Math.abs(body[key])>100000))return json({error:'Medición inválida'},400);
      const p={...body,device_id:id,mac,serial_number:serialNumber};delete p.claim_code;
      const {data,error}=await service.rpc('ingest_device_telemetry',{p});
      if(error)throw error;
      return json(data);
    }
    const record={id,mac,serial_number:serialNumber,firmware:String(body.firmware||''),last_seen:new Date().toISOString(),metadata:{ip:body.ip||'',chip_model:body.chip_model||'ESP32',flash_size:body.flash_size||0}};
    const query=existing?service.from('device_inventory').update(record).eq('id',id):service.from('device_inventory').insert({...record,claim_code:claimCode,secret_hash:secretHash});
    const {error}=await query;if(error)throw error;
    return json({ok:true,device_id:id,status:existing?'known':'registered'},existing?200:201);
  }catch(error){return json({error:error instanceof Error?error.message:String(error)},400)}
});

