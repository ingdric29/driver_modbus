# Devlog - Sistema Registrador de Variables

**Versión**: 1.1.4  
**Estado**: Depurando
**Fecha de última revisión**: 29/08/2026 

## Introducción

Devlog es un sistema multiplataforma para registrar variables (eléctricas, mecánicas, informáticas, entre otras) en una base de datos, permitiendo su análisis posterior. Se compone de dos módulos principales:
1. Lectura y registro de datos en la base de datos, procesamiento de alarmas.
2. Visualización de datos a través de una interfaz web dinámica.

Compatibilidad:
- **Protocolos soportados**: Modbus TCP, Modbus RTU, RS232.
- **Plataformas**: Linux (Debian, Raspbian) y Windows 10.

---

## Requisitos

### Hardware
- **Raspberry Pi 2/3/4** o superior con 4 GB de RAM y SSD clase 10 de 8 GB.
- **PC/Laptop**: 32 o 64 bits, 2 GB RAM, 100 GB de disco duro.
- **Máquina Virtual**: Acceso a red (NIC) y 20 GB de disco.

### Software
- Servidor de bases de datos: **MySQL Server** o **MariaDB**.
- Servidor web: **Apache**.
- Cliente MySQL: **phpMyAdmin**.
- Intérprete: **PHP**.

---

## Instalación

### Linux (Debian/Raspbian)
1. **Instalar herramientas de desarrollo**:
   ```bash
    sudo apt install build-essential
   ```
  
2. **Configurar el servidor Apache**:
  ```bash
  sudo apt install apache2
  sudo systemctl start apache2
  ```
3. **Instalar y configurar MySQL o MariaDB**:
  ```bash
  sudo apt install mariadb-server mariadb-client
  sudo systemctl start mariadb
  ```
4. **Instalar dependencias adicionales**:

- Librerías: `libmysqlcppconn`, `phpmyadmin`, `curl`, `libmodbus`.
- Usar el gestor de paquetes para instalarlas si no se encuentran preinstaladas.

5. **Compilar el programa**:
  ```bash
  sudo make
  ```

6. **Mover las páginas dinámicas**:

Copiar el contenido de la carpeta site_devlog a /var/www/html/.

### En Windows 10

1. **Instalar AppServ**:

Descarga desde AppServ para instalar Apache, PHP, MySQL y phpMyAdmin.
Mover las páginas dinámicas: Copiar la carpeta `site_devlog` a `C:\AppServ\www.`.

2. **Ejecutar el programa**:

Navegar a la carpeta Release y ejecutar devlogw.exe.
Alternativamente, compilar con Visual Studio 2019 si no tienes el ejecutable.

## Configuración

El archivo principal de configuración es configdb.cfg, ubicado junto al ejecutable. Algunos parámetros clave son:

- **Conexión a la base de datos**:
  ```ini
  Copiar código
  [DATABASE]
  db_nombre = devlogplus
  db_direccion = 127.0.0.1
  db_usuario = sigge
  db_clave = automa2424
  ```
- **Períodos de muestreo**:
  ```ini
  Copiar código
  periodo_muestreo_tbejecutar = 30
  periodo_muestreo_tbcambios = 20

- **SMTP para notificaciones**:
  ```ini
  smtp_from = diomede3009@gdingenieria.net
  smtp_servidor = smtps://mail.gdingenieria.net

## Base de Datos

1. **Importar estructura**:

- Accede a `http://localhost:8080/phpmyadmin` y usa la opción "Importar".
- Selecciona el archivo `bd devlogplus.sql` y ejecuta la importación.

2. **Crear un usuario MySQL**:
Asegúrate de que el usuario coincida con el configurado en `configdb.cfg`.

## Uso
### En Linux
1- **Dar permisos de ejecución**:
  ```bash
  chmod +x devlogplus
  ```
2- **Ejecutar el programa**:
  ```bash
  ./devlogplus
  ```

### En Windows

Ejecuta directamente el archivo devlogw.exe desde la carpeta Release.

## Acceso Web

- **URL del sistema SCADA**:
  http://localhost:8080/site_devlog/portada.php
- **Credenciales predeterminadas**:
Usuario: admin
Clave: admin

## Autor
- Elaborado por: Ing. Alejandro Diomede
- Contacto: diomede3009@gdingenieria.net
- Depurado por: Ing. Javier Flores
