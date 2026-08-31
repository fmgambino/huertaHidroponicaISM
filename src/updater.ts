import type{UpdateManifest}from'./types';
export const PRODUCT='HUERTA_H2_PWA' as const;
export function validateManifest(value:unknown):asserts value is UpdateManifest{
 if(!value||typeof value!=='object')throw new Error('Manifiesto inválido');
 const m=value as Partial<UpdateManifest>;
 if(m.product!==PRODUCT)throw new Error('Producto incorrecto');
 if(!/^\d+\.\d+\.\d+$/.test(m.version??''))throw new Error('Versión inválida');
 if(!Array.isArray(m.files)||!m.files.length)throw new Error('No hay archivos declarados');
}
export function safeRelativePath(value:string):string{
 const path=value.replace(/\\/g,'/').replace(/^\.\//,'');
 if(!path||path.startsWith('/')||path.includes('../')||/^[A-Za-z]:/.test(path))throw new Error(`Ruta no permitida: ${path}`);
 return path;
}
