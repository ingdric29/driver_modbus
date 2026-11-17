"""Isolated MariaDB + actual mixed-protocol driver. No production credentials."""
import os, pathlib, socket, subprocess, sys, tempfile, time, json
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
    peer=None; driver=None; output=None
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
        output=open(base/'driver.log','w')
        driver=subprocess.Popen([str(binary)],cwd=base/'run',env=env,stdout=output,stderr=output)
        prefix='USE driver_s7_fixture; '
        until(lambda: sql(prefix+"SELECT COUNT(DISTINCT dispositivo) FROM tbvalores")=='2')
        until(lambda: sql(prefix+"SELECT COUNT(*) FROM tbinstantaneos WHERE dispositivo=52")=='3')
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
