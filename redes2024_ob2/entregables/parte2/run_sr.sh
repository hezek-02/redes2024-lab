#!/bin/sh

# Verifica que se hayan pasado los parámetros necesarios
if [ $# -ne 3 ]; then
    echo "Usage: $0 mininet_machine_ip vhost_num mode"
    exit 1
fi

MININET_IP=$1
VHOST_NUM=$2
MODE=$3  # Puede ser 'enrutamiento' o 'reenvio'

if [ $MODE = "enrutamiento" ]; then
    echo "Ejecutando enrutamiento"
    sudo ./enrutamiento/sr -t 300 -v $VHOST_NUM -r e_rtable.$VHOST_NUM -s $MININET_IP -p 8888 -l e_$VHOST_NUM.pcap
elif [ $MODE = "reenvio" ]; then
    echo "Ejecutando reenvío"
    sudo ./reenvio/sr -t 300 -v $VHOST_NUM -r rtable.$VHOST_NUM -s $MININET_IP -p 8888 -l r_$VHOST_NUM.pcap
elif [ $MODE = "enrutamientoSol" ]; then
    echo "Ejecutando enrutamiento modelo"
    sudo ./sr_solution_ospf -t 300 -v $VHOST_NUM -r e_rtable.$VHOST_NUM -s $MININET_IP -p 8888 -l es_$VHOST_NUM.pcap
elif [ $MODE = "reenvioSol" ]; then
    echo "Ejecutando reenvío modelo"
    sudo ./sr_solution -t 300 -v $VHOST_NUM -r rtable.$VHOST_NUM -s $MININET_IP -p 8888 -l rs_$VHOST_NUM.pcap        
else
    echo "El modo debe ser 'enrutamiento' o 'reenvio'"
    exit 1
fi
