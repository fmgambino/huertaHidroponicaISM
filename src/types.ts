export type DatabaseRoleId='superadmin'|'alumno'|'docente'|'ecociudadano';
export type RoleId='superadmin'|'alumnos'|'docentes'|'ecociudadanos';
export type Permission='dashboard'|'status'|'sensors'|'history'|'actuators'|'camera'|'devices'|'users'|'roles'|'notifications'|'profile'|'team'|'mqtt'|'settings'|'audit'|'remote_reset';
export interface RoleProfile{id:RoleId;name:string;description:string;permissions:Permission[];locked?:boolean}
export interface Device{id:string;name:string;zone:string;online:boolean;wifi:number;ip:string;cameraUrl:string;lastSeen:string}
export interface UpdateManifest{product:'HUERTA_H2_PWA';version:string;minimum_version:string;notes?:string;files:string[]}
export interface AppSettings{title:string;subtitle:string;timezone:string;theme:'system'|'light'|'dark';primary:string;sky:string;eco:string}
