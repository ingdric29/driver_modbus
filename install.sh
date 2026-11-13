#!/bin/bash
FECHA=`date +%Y-%m-%d`
HORA=`date +%H-%M-%S`
echo "$FECHA $HORA" | tee -a install_log.txt
echo "INICIO DE LA INSTALACION"  | tee -a install_log.txt
if [ $(whoami) = "root" ]
then
    echo "instalacion ejecutada como usuario root." | tee -a install_log.txt
else
    echo "instalacion ejecutada como usuario nomal puede que tenga ciertas restricciones." | tee -a install_log.txt
fi
echo "------------------------------------------------"  | tee -a install_log.txt
echo "*-instalacion de la herramientas de programacion"  | tee -a install_log.txt
echo "------------------------------------------------"  | tee -a install_log.txt
apt install build-essential
if [[ $? -eq 0 ]]
then
    echo "instalacion correcta de install build-essential resultado = $?"  | tee -a install_log.txt
else
    echo "falla en la instalacion de install build-essential - resultado =$?"  | tee -a install_log.txt
fi
echo "verificando version del grupo de aplicaciones build-essential:"  | tee -a install_log.txt
apt show build-essential | grep Version | tee -a install_log.txt
echo "*-Instalacion del servidor web"  | tee -a install_log.txt
echo "-----------------------------"  | tee -a install_log.txt
apt install apache2
if [[ $? -eq 0 ]]
then
    echo "instalacion correcta de apache2 - resultado = $?"  | tee -a install_log.txt
else
    echo "falla en la instalacion de apache2 - resultado =$?"  | tee -a install_log.txt
fi
echo "verificando version del servidor web:"  | tee -a install_log.txt
apt show apache2 | grep Version | tee -a install_log.txt
systemctl start apache2
echo "*-instalacion del motor de bases de datos"  | tee -a install_log.txt
echo "----------------------------------------"  | tee -a install_log.txt
apt install mariadb-server
if [[ $? -eq 0 ]]
then
    echo "instalacion correcta de mariadb-server - resultado = $?"  | tee -a install_log.txt
else
    echo "falla en la instalacion de mariadb-server - resultado =$?"  | tee -a install_log.txt
fi
apt install mariadb-client
if [[ $? -eq 0 ]]
then
    echo "instalacion correcta de mariadb-client - resultado = $?"  | tee -a install_log.txt
else
    echo "falla en la instalacion de mariadb-client - resultado =$?"  | tee -a install_log.txt
fi

echo "*-instalacion de la libreria de modbus  "  | tee -a install_log.txt
echo "----------------------------------------"  | tee -a install_log.txt
apt install libmodbus
if [[ $? -eq 0 ]]
then
    echo "instalacion correcta de libmodbus - resultado = $?"  | tee -a install_log.txt
else
    echo "falla en la instalacion de libmodbus - resultado =$?"  | tee -a install_log.txt
fi
echo "verificando version de la libmodbus:"  | tee -a install_log.txt
apt show libmodbus | grep Version | tee -a install_log.txt

nombrehub=""
until [[ $nombrehub != "" ]]
do
	read -p 'indique el nombre del hub: ' nombrehub
	if [[ $nombrehub != "" ]]; then
		sed "s/remnombrehub/$nombrehub/" mkconfigdb.cfg > configdb.cfg
	else
	echo "nombre vacio"
	fi
done
mariadb -h localhost <<EOF
source mk_devlog_8_10_2023.sql
CREATE USER IF NOT EXISTS 'automatroni'@'%' IDENTIFIED BY 'automa2424';
GRANT ALL PRIVILEGES ON *.* TO 'automatroni'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;
EOF