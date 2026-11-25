"""Isolated MariaDB + actual mixed-protocol driver. No production credentials."""
import os, pathlib, socket, subprocess, sys, tempfile, time, json
from smtp_fixture import Server, certificate
ALARMS = "--alarms-events" in sys.argv[3:]
root=pathlib.Path(__file__).resolve().parents[1]
binary=pathlib.Path(sys.argv[1]).resolve()
fixture_binary=pathlib.Path(sys.argv[2]).resolve()
sys.path.insert(0,str(root/'tools'))
import importlib.util
spec=importlib.util.spec_from_file_location('viewer',root/'tools/automonitorctl.py')
viewer=importlib.util.module_from_spec(spec);spec.loader.exec_module(viewer)
with tempfile.TemporaryDirectory(prefix='s7-sql-') as directory:
    base=pathlib.Path(directory); sysroot=pathlib.Path(os.environ.get('S7_TEST_SYSROOT','/usr'))
    env=os.environ.copy(); env['PATH']=str(sysroot/'bin')+':'+env['PATH']
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1',0)); port=reserve.getsockname()[1]
    subprocess.run([str(sysroot/'bin/mariadb-install-db'),'--no-defaults','--basedir='+str(sysroot),
                    '--datadir='+str(base/'data'),'--skip-test-db','--auth-root-authentication-method=normal'],
                   env=env,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    log=open(base/'database.log','w')
    db=subprocess.Popen([str(sysroot/'sbin/mariadbd'),'--no-defaults','--basedir='+str(sysroot),
        '--datadir='+str(base/'data'),'--socket='+str(base/'db.sock'),'--pid-file='+str(base/'pid'),
        '--bind-address=127.0.0.1','--port='+str(port),'--innodb-buffer-pool-size=32M'],env=env,stdout=log,stderr=log)
    peer=None; driver=None; output=None; smtp=None
    def sql(text):
        return subprocess.check_output([str(sysroot/'bin/mariadb'),'--no-defaults','-h127.0.0.1',
               '-P'+str(port),'-uroot','-N','-e',text],env=env,text=True,stderr=subprocess.PIPE).strip()
    def until(check,seconds=15):
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if driver is not None and driver.poll() is not None:
                raise AssertionError((base/'driver.log').read_text())
            try:
                if check(): return
            except (subprocess.CalledProcessError,OSError,ValueError): pass
            time.sleep(.05)
        raise AssertionError('Timeout: '+(base/'driver.log').read_text() if (base/'driver.log').exists() else 'database startup timeout')
    try:
        until(lambda: sql('SELECT 1')=='1')
        peer=subprocess.Popen([str(fixture_binary)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,env=env)
        s7port,mbport,_=map(int,peer.stdout.readline().split())
        setup=(root/'tests/fixtures/startup_alarm.sql').read_text().replace('driver_startup_fixture','driver_s7_fixture')
        setup+=f"""
USE driver_s7_fixture;
DELETE FROM tbdefalarmas;
ALTER TABLE tbdispositivos ADD rack INT NULL, ADD slot INT NULL;
INSERT INTO qrytablacom (id,registro,tag,puerto) VALUES (1,0,'voltage',{mbport});
INSERT INTO tbdispositivos (id,nombre,direccion_ip,puerto,modelo,protocolo,habilitar,concentrador,rack,slot)
VALUES (52,'Siemens_S7_1200','127.0.0.1',{s7port},'S7-1200_Rack','SIEMENS_S7',1,'startup-test',0,1);
CREATE TABLE tbregistros(id INT PRIMARY KEY,modelo VARCHAR(20),tag VARCHAR(20),direccion_siemens VARCHAR(40),interpretacion VARCHAR(15),periodo FLOAT,escalamiento FLOAT,unidad VARCHAR(5),registrable INT,solocambios INT);
INSERT INTO tbregistros VALUES
(1,'S7-1200_Rack','bool','DB100.DBX0.0','s7_bool',0.1,1,'',0,0),
(2,'S7-1200_Rack','real','DB100.DBD8','s7_real',0.1,1,'C',1,1),
(3,'S7-1200_Rack','dword','DB100.DBD12','s7_dword',0.1,1,'',1,0);
"""
        if ALARMS:
            cert, key = certificate(base)
            smtp = Server(cert, key, expected_auth=('fixture-user', 'fixture-secret'))
            setup += f"""
UPDATE tbdispositivos SET puerto={mbport} WHERE nombre='meter';
INSERT INTO qrytablacom (id,registro,tag,puerto) VALUES (2,5,'action',{mbport});
INSERT INTO tbcmd VALUES
(9000,5,128,NULL,0,'startup-test','alarm','meter'),
(9001,5,0,NULL,0,'startup-test','alarm','meter');
INSERT INTO tbdirectorio VALUES
(8,'driver@example.invalid','mixed@example.invalid','','mixed',''),
(9,'driver@example.invalid','modbus@example.invalid','','modbus',''),
(10,'driver@example.invalid','failed-action@example.invalid','','failed action','');
INSERT INTO tbdefalarmas VALUES
(42,'$(Siemens_S7_1200.real)>25.5','$(Siemens_S7_1200.real)<25.2','S7 REAL','CMD[9000];EMAIL[7];','CMD[9001];EMAIL[7];','Siemens_S7_1200','real',1,'real'),
(43,'$(meter.voltage)>1300 & $(Siemens_S7_1200.bool)=0','$(meter.voltage)<1300 | $(Siemens_S7_1200.bool)=1','mixed protocols','EMAIL[8];','EMAIL[8];','meter','voltage',1,'voltage'),
(44,'$(Siemens_S7_1200.bool)=1','$(Siemens_S7_1200.bool)=0','S7 BOOL','CMD[99999];EMAIL[10];','CMD[99999];EMAIL[10];','Siemens_S7_1200','bool',1,'bool'),
(45,'$(Siemens_S7_1200.dword)>2147483648','$(Siemens_S7_1200.dword)<2147483648','S7 DWORD','','','Siemens_S7_1200','dword',1,'dword'),
(46,'$(meter.voltage)>1300','$(meter.voltage)<1300','Modbus independent','EMAIL[9];','EMAIL[9];','meter','voltage',1,'voltage');
ALTER TABLE tbdefeventos ADD nombredispositivo VARCHAR(20), ADD tag VARCHAR(20), ADD bit1 INT, ADD activado TEXT, ADD desactivado TEXT;
INSERT INTO tbdefeventos VALUES
(1,1,'Siemens_S7_1200','bool',0,'bool ON','bool OFF'),
(2,1,'Siemens_S7_1200','dword',31,'bit31 ON','bit31 OFF'),
(3,1,'Siemens_S7_1200','real',-1,'REAL ON','REAL cambio'),
(4,1,'Siemens_S7_1200','dword',-1,'DWORD ON','DWORD cambio'),
(5,1,'Siemens_S7_1200','real',0,'invalid','invalid');
CREATE TABLE tbhistoeventos(id INT AUTO_INCREMENT PRIMARY KEY,id_defeventos INT,fechatiempo DATETIME,tiempo DATETIME,concentrador TEXT,nombredispositivo TEXT,tag TEXT,bit1 INT,mensaje TEXT);
"""
        sql(setup)
        (base/'run').mkdir()
        (base/'configdb.cfg').write_text(f'''[DATABASE]
db_direccion=127.0.0.1
db_puerto={port}
db_nombre=driver_s7_fixture
db_usuario=root
db_clave=
nombre_concentrador=startup-test
habilitar_acceso_externo=0
periodo_minimo_escritura_tbvalores=0.1
periodo_muestreo_tbejecutar=1
periodo_muestreo_tbcambios=1
''')
        if ALARMS:
            with (base/'configdb.cfg').open('a') as cfg:
                cfg.write(f"\nperiodo_revision_alarmas=0.1\nGLOBAL_periodo_revision_eventos=1\nsmtp_from=sender@example.invalid\nsmtp_usuario=fixture-user\nsmtp_pass=fixture-secret\nsmtp_servidor=smtps://127.0.0.1:{smtp.port}\n")
            (base/'html').mkdir()
            (base/'html/plantilla_emails.html').write_bytes((root/'html/plantilla_emails.html').read_bytes())
        output=open(base/'driver.log','w')
        driver=subprocess.Popen([str(binary)],cwd=base/'run',env=env,stdout=output,stderr=output)
        prefix='USE driver_s7_fixture; '
        if ALARMS:
            def command(c):
                peer.stdin.write(c+'\n'); peer.stdin.flush()
            def count(query):
                return int(sql(prefix + query))
            def received(to):
                return smtp.envelopes.count(('RCPT TO:<'+to+'@example.invalid>').encode())
            def history(id):
                return sql(prefix+f"SELECT GROUP_CONCAT(tipo ORDER BY id) FROM tbhistoalarmas WHERE id_alarma={id}")
            until(lambda: history(44)=='activacion' and history(45)=='activacion')
            assert sql(prefix+"SELECT valor FROM tbhistoalarmas WHERE id_alarma=45 LIMIT 1").startswith('4294967295')
            # First event baseline needs at least one event scan; no startup transition.
            time.sleep(2.2)
            assert count('SELECT COUNT(*) FROM tbhistoeventos')==0
            command('v')  # REAL 25 -> 26 and DWORD max -> 42.
            until(lambda: received('alarm7')==1)
            until(lambda: count('SELECT COUNT(*) FROM tbhistoeventos WHERE id_defeventos IN (2,3,4)')==3)
            assert 'CMD[9000]' in sql(prefix+"SELECT accion FROM tbhistoalarmas WHERE id_alarma=42 LIMIT 1")
            assert count("SELECT COUNT(*) FROM tbhistoeventos WHERE id_defeventos=2 AND mensaje='bit31 OFF - (0)'")==1
            command('r')  # 25.6 stays active; event keeps decimal precision.
            until(lambda: count("SELECT COUNT(*) FROM tbhistoeventos WHERE id_defeventos=3 AND mensaje='REAL cambio - (25.6)'")==1)
            assert received('alarm7')==1
            until(lambda: sql(prefix+"SELECT valorfloat FROM tbinstantaneos WHERE nombredispositivo='meter' AND tag='action'")=='128')
            command('h')
            until(lambda: count("SELECT COUNT(*) FROM tbhistoeventos WHERE id_defeventos=4 AND mensaje='DWORD cambio - (65536)'")==1)
            command('H')
            until(lambda: count("SELECT COUNT(*) FROM tbhistoeventos WHERE id_defeventos=4 AND mensaje='DWORD cambio - (131072)'")==1)
            assert count('SELECT COUNT(*) FROM tbhistoeventos WHERE id_defeventos=2')==1
            command('l')
            until(lambda: received('alarm7')==2)
            assert history(42)=='activacion,desactivacion'
            until(lambda: sql(prefix+"SELECT valorfloat FROM tbinstantaneos WHERE nombredispositivo='meter' AND tag='action'")=='0')
            command('b')
            until(lambda: history(44)=='activacion,desactivacion' and received('failed-action')==2)
            until(lambda: count("SELECT COUNT(*) FROM tbhistoeventos WHERE id_defeventos=1 AND mensaje='bool OFF - (0)'")==1)
            command('x')
            until(lambda: next(d for d in viewer.fetch(base/'.runtime/automonitor.sock')['devices'] if d['protocol']=='SIEMENS_S7')['status']!='ONLINE')
            before_events=count("SELECT COUNT(*) FROM tbhistoeventos")
            before_history=count("SELECT COUNT(*) FROM tbhistoalarmas WHERE dispositivo='Siemens_S7_1200'")
            command('m')
            until(lambda: received('modbus')==1)
            time.sleep(1.2)
            assert received('mixed')==0
            assert count('SELECT COUNT(*) FROM tbhistoeventos')==before_events
            assert count("SELECT COUNT(*) FROM tbhistoalarmas WHERE dispositivo='Siemens_S7_1200'")==before_history
            command('v'); command('s')
            until(lambda: received('alarm7')==3 and received('mixed')==1)
            assert history(42)=='activacion,desactivacion,activacion'
            until(lambda: count('SELECT COUNT(*) FROM tbhistoeventos')>before_events)
            # Removing S7 on reload must discard its registry/references safely.
            generation=viewer.fetch(base/'.runtime/automonitor.sock')['generation']
            sql(prefix+"UPDATE tbdispositivos SET habilitar=0 WHERE id=52; UPDATE tbcambios SET cont_tbdispositivos=1 WHERE id=1")
            until(lambda: (lambda snap: snap['generation']>generation and {d['protocol'] for d in snap['devices']}=={'MBTCP'})(viewer.fetch(base/'.runtime/automonitor.sock')),25)
            time.sleep(1.2)
            assert received('alarm7')==3
            assert count('SELECT COUNT(*) FROM tbhistoeventos WHERE id_defeventos=5')==0
            assert 'Evento Siemens 5' in (base/'log/devlog.log').read_text()
            driver.terminate(); assert driver.wait(timeout=12)==0
            print('PASS S7 alarms/events: BOOL/REAL/DWORD, bit31, decimal events, both alarm emails, mixed references, Modbus during S7 outage, no false transitions, recovery and catalog removal; loopback only')
            sys.exit(0)
        until(lambda: sql(prefix+"SELECT COUNT(DISTINCT dispositivo) FROM tbvalores")=='2')
        until(lambda: sql(prefix+"SELECT COUNT(*) FROM tbinstantaneos WHERE dispositivo=52")=='3')
        startup=(base/'driver.log').read_text()
        assert any(line.split()==['Tags','Siemens','S7','3'] for line in startup.splitlines()), startup
        assert any(line.split()==['PLC','Siemens','S7','1'] for line in startup.splitlines()), startup
        assert sql(prefix+"SELECT valorfloat FROM tbinstantaneos WHERE dispositivo=52 AND tag='dword'")=='4294967295'
        assert sql(prefix+"SELECT COUNT(*) FROM tbvalores WHERE dispositivo=52 AND tag='bool'")=='0'
        assert sql(prefix+"SELECT COUNT(*) FROM tbvalores WHERE dispositivo=52 AND tag='real'")=='1'
        time.sleep(.4)
        assert sql(prefix+"SELECT COUNT(*) FROM tbvalores WHERE dispositivo=52 AND tag='real'")=='1'
        trace=sql(prefix+"SELECT rastro FROM tbinstantaneos WHERE dispositivo=52 AND tag='real'")
        assert ';protocolo=SIEMENS_S7;rack=0;slot=1;dir=DB100.DBD8;tipo=s7_real;esc=1' in trace,trace
        sql(prefix+"\nDELIMITER $$\nCREATE TRIGGER reject_dword BEFORE INSERT ON tbvalores FOR EACH ROW BEGIN IF NEW.tag='dword' THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='fixture failure'; END IF; END$$\nDELIMITER ;")
        peer.stdin.write('v\n');peer.stdin.flush()
        until(lambda: sql(prefix+"SELECT valorfloat FROM tbinstantaneos WHERE dispositivo=52 AND tag='real'")=='26')
        time.sleep(.2)
        assert sql(prefix+"SELECT valorfloat FROM tbinstantaneos WHERE dispositivo=52 AND tag='dword'")=='4294967295'
        sql(prefix+'DROP TRIGGER reject_dword')
        until(lambda: sql(prefix+"SELECT valorfloat FROM tbinstantaneos WHERE dispositivo=52 AND tag='dword'")=='42')
        assert sql(prefix+"SELECT COUNT(*) FROM tbvalores WHERE dispositivo=52 AND tag='real'")=='2'
        peer.stdin.write('r\n');peer.stdin.flush()
        until(lambda: sql(prefix+"SELECT CONCAT(valorstr,'|',valorfloat) FROM tbinstantaneos WHERE dispositivo=52 AND tag='real'")=='25.6|25.6')
        until(lambda: sql(prefix+"SELECT CONCAT(valorstr,'|',valorfloat) FROM tbvalores WHERE dispositivo=52 AND tag='real' ORDER BY id DESC LIMIT 1")=='25.6|25.6')
        observed=viewer.fetch(base/'.runtime/automonitor.sock')
        assert next(t for d in observed['devices'] if d['protocol']=='SIEMENS_S7' for t in d['tags'] if t['name']=='real')['value']=='25.6'
        before=sql(prefix+"SELECT COUNT(*) FROM tbvalores WHERE dispositivo=52")
        peer.stdin.write('x\n');peer.stdin.flush();time.sleep(.5)
        before=sql(prefix+"SELECT COUNT(*) FROM tbvalores WHERE dispositivo=52")
        time.sleep(.4)
        assert sql(prefix+"SELECT COUNT(*) FROM tbvalores WHERE dispositivo=52")==before
        snapshot=viewer.fetch(base/'.runtime/automonitor.sock')
        assert {d['protocol'] for d in snapshot['devices']}=={'MBTCP','SIEMENS_S7'}
        assert next(d for d in snapshot['devices'] if d['protocol']=='MBTCP')['status']=='ONLINE'
        driver.terminate();assert driver.wait(timeout=10)==0
        print('PASS mixed SQL: catalog, real IDs, trace, DWORD, upsert, change-only, nonregistrable, rollback/retry, PLC failure and shutdown')
    finally:
        if driver is not None and driver.poll() is None: driver.terminate();driver.wait(timeout=12)
        if peer is not None: peer.communicate('q',timeout=5)
        db.terminate();db.wait(timeout=15);log.close()
        if output:output.close()
        if smtp:smtp.close()
