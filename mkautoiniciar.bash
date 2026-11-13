#!/bin/bash

function ejecutar() {
	FECHA=`date +%Y-%m-%d`
	HORA=`date +%H-%M-%S`
	echo "activando el programa devlogplus"
	sudo killall devlogplus
	sudo killall automonitorv1
	sudo killall orquesta
	cd /home/pi/automonitor1
	nohup /home/pi/automonitor1/orquesta &
	env >> "variablesenv"
	date >> "fechaejecucion" 
}

ejecutar
exit
