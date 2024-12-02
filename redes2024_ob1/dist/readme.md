# Obligatorios REDES2024

## Parte 1

Se debe tener una red emulada, como la dispuesta por la máquina virtual dada, o bien correr el sistema Mininet de la Máquina virtual dada.
Para ello se dispone de comandos para levantar la red emulada y realizar las pruebas sugeridas. Si se desea probar parcialmente la implementación, es necesario cambiar la IP de los servidores a 127.0.0.1 y eliminar la conexión con el servidor ajeno, así como sus pruebas.

Descomprimir tar.gz, o copiar directorio actual en redes2024_ob1/dist
### Configuración previa

#### De host a VM (se puede omitir si se tiene una interfaz gráfica)

```
ssh -X -p 2522 osboxes@127.0.0.1
```

#### Terminal 1

```
./config.sh
```

#### Terminal 2

```
./run_mininet.sh
```

#### Terminal 3

```
./run_pox.sh
```

#### Terminales 4, 5, 6

```
./run_sr.sh 127.0.0.1 vhost1
./run_sr.sh 127.0.0.1 vhost2
./run_sr.sh 127.0.0.1 vhost3
```

#### Paso necesario, ya que se usa la IP y puerto para levantar el servidor ajeno

```
pkill -9 -f test-server
```

### Configuración de la red

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

### Si se desea debugear prints quitar nohup y &, luego cancelar la ejecución de servidores con ctrl + C

```
server1  python3.8 ./serverOne.py
```

```
server2  python3.8 ./serverTwo.py
```
