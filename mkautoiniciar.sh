#!/bin/bash

function ejecutar() {
	FECHA=`date +%Y-%m-%d`
	HORA=`date +%H-%M-%S`
	echo "activando el programa devlogplus v1.0"
	echo "-------------------------------------"
	sudo killall devlogplus
	sudo killall automonitorv1
	sudo killall orquesta
	cd remruta
	sudo nohup ./bin/orquesta &>"remruta/nohup.out"
	env >> "remruta/variablesenv"
	date >> "remruta/fechaejecucion" 
}

ejecutar
exit
