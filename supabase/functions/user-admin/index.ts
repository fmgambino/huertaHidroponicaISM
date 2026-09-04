import {createClient} from 'https://esm.sh/@supabase/supabase-js@2';

const cors={'Access-Control-Allow-Origin':'*','Access-Control-Allow-Headers':'authorization, x-client-info, apikey, content-type','Access-Control-Allow-Methods':'POST, OPTIONS'};
const json=(body:unknown,status=200)=>new Response(JSON.stringify(body),{status,headers:{...cors,'Content-Type':'application/json'}});

Deno.serve(async request=>{
  if(request.method==='OPTIONS')return new Response('ok',{headers:cors});
  try{
    const url=Deno.env.get('SUPABASE_URL')!,anon=Deno.env.get('SUPABASE_ANON_KEY')!,service=Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!;
    const authorization=request.headers.get('Authorization')||'';
    const callerClient=createClient(url,anon,{global:{headers:{Authorization:authorization}}});
    const {data:{user},error:userError}=await callerClient.auth.getUser();
    if(userError||!user)return json({error:'Sesión inválida'},401);
    const admin=createClient(url,service,{auth:{autoRefreshToken:false,persistSession:false}});
    const {data:profile}=await admin.from('profiles').select('role').eq('id',user.id).single();
    if(profile?.role!=='superadmin')return json({error:'Sólo SuperAdmin puede administrar credenciales'},403);
    const body=await request.json(),action=body.action;
    if(action==='list'){
      const {data,error}=await admin.auth.admin.listUsers({page:1,perPage:1000});if(error)throw error;
      const {data:profiles,error:profilesError}=await admin.from('profiles').select('*');if(profilesError)throw profilesError;
      return json({users:data.users.map(u=>{const p=profiles?.find(x=>x.id===u.id);return{id:u.id,email:u.email,name:p?.display_name||u.user_metadata?.name||'',role:p?.role||'ecociudadano',avatar:p?.avatar_url||'',active:!u.banned_until}})});
    }
    if(action==='create'){
      if(!body.email||!body.password||body.password.length<8)return json({error:'Email y contraseña de 8 caracteres son obligatorios'},400);
      const {data,error}=await admin.auth.admin.createUser({email:body.email,password:body.password,email_confirm:true,user_metadata:{name:body.name}});if(error)throw error;
      await admin.from('profiles').upsert({id:data.user.id,display_name:body.name||'',role:body.role||'ecociudadano',updated_at:new Date().toISOString()});return json({id:data.user.id});
    }
    if(action==='update'){
      const attributes:any={email:body.email,user_metadata:{name:body.name}};if(body.password)attributes.password=body.password;
      const {error}=await admin.auth.admin.updateUserById(body.id,attributes);if(error)throw error;
      const {error:profileError}=await admin.from('profiles').update({display_name:body.name,role:body.role,updated_at:new Date().toISOString()}).eq('id',body.id);if(profileError)throw profileError;return json({ok:true});
    }
    if(action==='delete'){const {error}=await admin.auth.admin.deleteUser(body.id);if(error)throw error;return json({ok:true});}
    return json({error:'Acción inválida'},400);
  }catch(error){return json({error:error instanceof Error?error.message:String(error)},400)}
});
