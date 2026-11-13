#!/usr/bin/env python3
"""Exercise real startup with legacy license fields absent, zero or malformed.

Use only an isolated config and a closed loopback port. Reaching acquisition
configuration is the success criterion; a database connection failure remains
an expected nonzero application exit, not proof of a running acquisition loop.
"""
import pathlib
import socket
import subprocess
import sys
import tempfile

binary = pathlib.Path(sys.argv[1]).resolve(strict=True)
with tempfile.TemporaryDirectory(prefix="driver-startup-") as temporary, socket.socket() as reservation:
    base = pathlib.Path(temporary)
    (base / "run").mkdir()
    reservation.bind(("127.0.0.1", 0))
    port = reservation.getsockname()[1]
    config = f"""[DATABASE]
db_direccion=127.0.0.1
db_puerto={port}
db_nombre=startup_test
db_usuario=startup_test
db_clave=
habilitar_acceso_externo=0
"""
    for label, legacy_keys in (
        ("absent", ""),
        ("zero", "clave1=0\nclave2=0\n"),
        ("malformed", "clave1=not-a-hex-key\nclave2=also-invalid\n"),
    ):
        (base / "configdb.cfg").write_text(config + legacy_keys)
        completed = subprocess.run(
            [str(binary)], cwd=base / "run", stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, timeout=10,
        )
        output = completed.stdout
        assert completed.returncode == 1, (label, completed.returncode, output)
        assert "INFO: Start Devlog." in output, (label, output)
        assert "SQL operation failed:" in output, (label, output)
        for rejected in ("Ambas claves invalidas", "Invalid license key format", "License not activated"):
            assert rejected not in output, (label, output)
        print(f"PASS startup without license: {label} (expected database unavailable)")
