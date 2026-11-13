CREATE DATABASE IF NOT EXISTS driver_startup_fixture;
USE driver_startup_fixture;
DROP TABLE IF EXISTS tbhistoalarmas,qrytablacom,tbcambios,tbcmd,tbdispositivos,tbdefalarmas,tbdirectorio,tbsms,tbdefeventos,tbdispositivos_sta,tbejecutar,tbvalores,tbinstantaneos;
CREATE TABLE qrytablacom (
 id INT, registro INT, tag TEXT, puerto INT,
 nombre TEXT DEFAULT 'meter', direccion_ip TEXT DEFAULT '127.0.0.1',
 direccion_esclavo INT DEFAULT 1, interface TEXT DEFAULT '', velocidad INT DEFAULT 9600,
 paridad TEXT DEFAULT 'N', bitsparada INT DEFAULT 1, equipo TEXT DEFAULT '0',
 usuario TEXT DEFAULT '', password TEXT DEFAULT '', habilitar INT DEFAULT 1,
 protocolo TEXT DEFAULT 'MBTCP', funcion TEXT DEFAULT 'hr', interpretacion TEXT DEFAULT 'word',
 escalamiento TEXT DEFAULT '1', periodo TEXT DEFAULT '0.1', bit1 INT DEFAULT -1,
 cantidad1 INT DEFAULT 1, comentario TEXT DEFAULT '', unidad TEXT DEFAULT 'V',
 solocambios INT DEFAULT 0, registrable INT DEFAULT 1, html_id TEXT DEFAULT '', html_final TEXT DEFAULT ''
);
CREATE TABLE tbcambios (id INT,cont_tbregistros INT,cont_tbdispositivos INT,cont_tbdefequipos INT,cont_tbdefalarmas INT);
INSERT INTO tbcambios VALUES(1,0,0,0,0);
CREATE TABLE tbdispositivos (id INT,nombre VARCHAR(64) PRIMARY KEY,direccion_ip TEXT,puerto INT,direccion_esclavo INT,interface TEXT,velocidad INT,paridad TEXT,bitsparada INT,modelo TEXT,usuario TEXT,password TEXT,protocolo TEXT,habilitar INT,concentrador TEXT);
INSERT INTO tbdispositivos VALUES(1,'meter','127.0.0.1',502,1,'',9600,'N',1,'0','','','MBTCP',1,'startup-test');
CREATE TABLE tbcmd (id INT,registro INT,valor INT,cmd_datetime DATETIME,hab_ejecutar INT,concentrador TEXT,lote TEXT,nombredispositivo TEXT);
CREATE TABLE tbejecutar (id INT,concentrador TEXT);
CREATE TABLE tbdefalarmas (id INT,activar TEXT,desactivar TEXT,descripcion TEXT,accionactivar TEXT,acciondesactivar TEXT,dispositivo TEXT,tag TEXT,habilitar INT,valorAlarma TEXT);
INSERT INTO tbdefalarmas VALUES(42,'$(meter.r0)>250','$(meter.r0)<240','voltage spike','EMAIL[7];','EMAIL[7];','meter','r0',1,'r0');
CREATE TABLE tbdirectorio (id INT,emailfrom TEXT,emailto TEXT,emailcc TEXT,asunto TEXT,mensaje TEXT);
INSERT INTO tbdirectorio VALUES(7,'driver@example.invalid','alarm7@example.invalid','','test','');
CREATE TABLE tbsms (id INT);
CREATE TABLE tbdefeventos (id INT,habilitar INT);
CREATE TABLE tbdispositivos_sta (id INT AUTO_INCREMENT PRIMARY KEY,nombre TEXT,errores INT,errores_acumulados INT,estado INT,fechatiempo DATETIME,concentrador TEXT);
CREATE TABLE tbhistoalarmas (id INT AUTO_INCREMENT PRIMARY KEY,id_alarma INT,fechatiempo DATETIME,tiempo DATETIME,tipo TEXT,descripcion TEXT,valor TEXT,accion TEXT,condicion TEXT,concentrador TEXT,dispositivo VARCHAR(64),tag TEXT,reconocida INT, FOREIGN KEY(dispositivo) REFERENCES tbdispositivos(nombre));
CREATE TABLE tbvalores (id BIGINT AUTO_INCREMENT PRIMARY KEY,timedate TIMESTAMP NULL,fechatiempo DATETIME,tag TEXT,valorstr TEXT,valorfloat DOUBLE,dispositivo INT,rastro TEXT,concentrador TEXT,nombredispositivo TEXT);
CREATE TABLE tbinstantaneos LIKE tbvalores;
