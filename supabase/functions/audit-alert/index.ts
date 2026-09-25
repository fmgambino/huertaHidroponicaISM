import {createClient} from 'https://esm.sh/@supabase/supabase-js@2';

const cors={'Access-Control-Allow-Origin':'*','Access-Control-Allow-Headers':'authorization, x-client-info, apikey, content-type, x-webhook-secret','Access-Control-Allow-Methods':'POST, OPTIONS'};
const reply=(body:unknown,status=200)=>new Response(JSON.stringify(body),{status,headers:{...cors,'Content-Type':'application/json'}});

Deno.serve(async request=>{
  if(request.method==='OPTIONS')return new Response('ok',{headers:cors});
  try{
    const url=Deno.env.get('SUPABASE_URL')!,service=Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!;
    const admin=createClient(url,service,{auth:{persistSession:false,autoRefreshToken:false}});
    const body=await request.json();
    const webhookSecret=Deno.env.get('AUDIT_WEBHOOK_SECRET')||'';
    const fromWebhook=request.headers.get('x-webhook-secret')===webhookSecret&&!!webhookSecret;
    let authorizedSuperAdmin=false;
    if(!fromWebhook){
      const token=request.headers.get('Authorization')||'';
      const caller=createClient(url,Deno.env.get('SUPABASE_ANON_KEY')!,{global:{headers:{Authorization:token}}});
      const {data:{user}}=await caller.auth.getUser();if(!user)return reply({error:'Sesión inválida'},401);
      const {data:profile}=await admin.from('profiles').select('role,display_name').eq('id',user.id).single();if(profile?.role!=='superadmin')return reply({error:'Sólo SuperAdmin'},403);
      authorizedSuperAdmin=true;
    }
    const record=body.record||body;
    const action=String(record.action||body.action||'');
    if(!/DELETE|ELIMINAR/i.test(action))return reply({ok:true,skipped:'No es una eliminación'});
    if(fromWebhook){
      if(!record.actor_id)return reply({ok:true,skipped:'Eliminación sin actor identificado'});
      if(record.details?.actor_role==='superadmin')authorizedSuperAdmin=true;
      else{const {data:actor}=await admin.from('profiles').select('role').eq('id',record.actor_id).maybeSingle();authorizedSuperAdmin=actor?.role==='superadmin'}
    }
    if(!authorizedSuperAdmin)return reply({ok:true,skipped:'El actor no es SuperAdmin'});
    const [{data:dedicated},{data:settings,error}]=await Promise.all([admin.from('app_settings').select('value').eq('id','emailjs').maybeSingle(),admin.from('app_settings').select('value').eq('id','global').maybeSingle()]);if(error)throw error;
    const cfg=dedicated?.value||settings?.value?.settings?.emailjs||{},templates=Array.isArray(cfg.templates)?cfg.templates:[];
    const template=templates.find((item:any)=>item.purpose==='deletion')||templates[0];
    if(!cfg.serviceId||!cfg.publicKey||!template?.id)throw Error('Configurá Service ID, Public key y un template de eliminación en la PWA.');
    const destination=Deno.env.get('ALERT_EMAIL')||'fernando.m.gambino@gmail.com';
    const details=record.details||body.detail||{};
    const params:any={to_email:destination,email:destination,recipient:destination,user_email:destination,to_name:'Fernando Gambino',subject:'Alerta de eliminación · Proyecto H²',title:'Alerta de eliminación',message:`${action}\nUsuario: ${body.actor_email||record.actor_id||'Sistema'}\nDetalle: ${JSON.stringify(details)}`,action,actor_email:body.actor_email||record.actor_id||'Sistema',details:JSON.stringify(details),created_at:record.created_at||new Date().toISOString()};
    params[template.recipientParam||'email']=destination;
    const payload:any={service_id:cfg.serviceId,template_id:template.id,user_id:cfg.publicKey,template_params:params};
    const privateKey=Deno.env.get('EMAILJS_PRIVATE_KEY');if(privateKey)payload.accessToken=privateKey;
    const response=await fetch('https://api.emailjs.com/api/v1.0/email/send',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});
    const text=await response.text();if(!response.ok)throw Error(`EmailJS ${response.status}: ${text}`);
    return reply({ok:true});
  }catch(error){return reply({error:error instanceof Error?error.message:String(error)},400)}
});
