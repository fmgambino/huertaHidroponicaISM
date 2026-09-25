const SHELL='huerta-h2-shell-v2.14.2';
const ACTIVE='huerta-patch-active';
const ASSETS=['./','./index.html','./assets/css/app.css?v=2.14.2','./assets/css/v21.css?v=2.14.2','./assets/js/config.js?v=2.14.2','./assets/js/app.js?v=2.14.2','./assets/icons/icon.svg','./manifest.webmanifest'];

self.addEventListener('install',event=>event.waitUntil(
  caches.open(SHELL).then(cache=>cache.addAll(ASSETS)).then(()=>self.skipWaiting())
));

self.addEventListener('activate',event=>event.waitUntil((async()=>{
  // Limpia shells, staging y respaldos obsoletos. El patch activo se conserva
  // porque es el mecanismo de actualización local de la PWA.
  for(const name of await caches.keys()){
    if(name!==SHELL&&name!==ACTIVE)await caches.delete(name);
  }
  await self.clients.claim();
})()));

self.addEventListener('fetch',event=>{
  if(event.request.method!=='GET')return;
  const url=new URL(event.request.url);
  if(url.origin!==self.location.origin)return;
  event.respondWith((async()=>{
    const override=await (await caches.open(ACTIVE)).match(event.request,{ignoreSearch:true});
    if(override)return override;
    try{
      const fresh=await fetch(event.request,{cache:'no-store'});
      if(fresh.ok)(await caches.open(SHELL)).put(event.request,fresh.clone());
      return fresh;
    }catch{
      return await caches.match(event.request,{ignoreSearch:true})||await caches.match('./index.html');
    }
  })());
});
