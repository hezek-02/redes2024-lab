# Obligatorios REDES2024

## Parte 2

En esta parte solo se contempla la disposición de un cliente hacía dos servidores con la ip '100.100.0.2' '150.150.0.2'


#### De host a VM (se puede omitir si se tiene una interfaz gráfica)

```
ssh -X -p 2522 osboxes@127.0.0.1
```

### Si desea eliminar los servicios de servidores
```
pkill -9 -f serverOne && pkill -9 -f serverTwo
```

### Configuración de la red
- Esta configuración en el inicio, no es necesaria, ya que el archivo pwospf_topo.py ya los levanta

```
server1 nohup python3.8 ./serverOne.py &
```

```
server2 nohup python3.8 ./serverTwo.py &
```

### Ejecución del cliente

```
client cd dist/; python3.8 clientTest.py
```

### Ejecución de múltiples clientes

```
client python3.8 clientTest.py & python3.8 clientTest.py & python3.8 clientTest.py & python3.8 clientTest.py
```

### Extras

```
vhost1 nohup sudo tcpdump -n -i vhost1-eth1 -w vhost1.pcap &
```

```
vhost1 killall tcpdump
```

```
client traceroute -n 200.0.0.10
```

```
client ping -c 3 200.0.0.10
```

