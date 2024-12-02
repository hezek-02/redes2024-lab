# Obligatorios REDES2024

## Instrucciones para Compilar y Ejecutar el Código

Puede compilar el código de la siguiente manera:
```sh
cd ~/redes2024_ob2/reenvio
cd ~/redes2024_ob2/enrutamiento

make
```

Esto genera un ejecutable `sr`. Para ejecutarlo en cada router deberá ejecutar `run_sr.sh` con los parámetros pertinentes `ip` `n_router` `modo`.
Se especifica más abajo, el modo identifica si emplea pwospf o se tratan de tablas estaticas

## TOPOLOGIAS

- En cada entrega está disponible la nueva topologia de pruebas (la de 5 routers), si desea emplearla debe sustituir `todos` los archivos en la raíz del proyecto, si no, solo emplee `enrutamiento` o `reenvio`, esto último empleará solo la topología de 3 routers. 

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

#### Terminales 4, 5, 6, 7, 8
- Los ejecutados con el sufijo Sol son ejecutables de solución modelo, los cuales son:
- - sr_solution
- - sr_solution_ospf   
- Cada ejecucion tiene su propia salida captura, exportados en redes2024_ob2 (root del proj) archivos .pcap:
- - prefijo e: corresponde a nuestro binario de parte2 (enrutamiento)
- - prefijo es: corresponde a binario modelo de parte2 (enrutamientoSol)
- - prefijo r: corresponde a nuestro binario de parte1 (reenvio)
- - prefijo rs: corresponde a binario modelo de parte1 (reenvioSol)

#### Parte 1, forwarding
```
./run_sr.sh 127.0.0.1 vhost1 reenvio
./run_sr.sh 127.0.0.1 vhost2 reenvio
./run_sr.sh 127.0.0.1 vhost3 reenvio
./run_sr.sh 127.0.0.1 vhost4 reenvio
./run_sr.sh 127.0.0.1 vhost5 reenvio
```
### Parte 1, forwarding ejecución con solución modelo
```
./run_sr.sh 127.0.0.1 vhost1 reenvioSol
./run_sr.sh 127.0.0.1 vhost2 reenvioSol
./run_sr.sh 127.0.0.1 vhost3 reenvioSol
./run_sr.sh 127.0.0.1 vhost4 reenvioSol
./run_sr.sh 127.0.0.1 vhost5 reenvioSol
```
#### Parte 2, forwarding y routing (pwospf)
```
./run_sr.sh 127.0.0.1 vhost1 enrutamiento
./run_sr.sh 127.0.0.1 vhost2 enrutamiento
./run_sr.sh 127.0.0.1 vhost3 enrutamiento
./run_sr.sh 127.0.0.1 vhost4 enrutamiento
./run_sr.sh 127.0.0.1 vhost5 enrutamiento
```

#### Parte 2, forwarding y routing (pwospf) ejecución con solución modelo

```
./run_sr.sh 127.0.0.1 vhost1 enrutamientoSol
./run_sr.sh 127.0.0.1 vhost2 enrutamientoSol
./run_sr.sh 127.0.0.1 vhost3 enrutamientoSol
./run_sr.sh 127.0.0.1 vhost4 enrutamientoSol
./run_sr.sh 127.0.0.1 vhost5 enrutamientoSol

```

#### Ejecución aplicación cliente-servidor

La configuración de los servidores (su aplicación) ya está definida al iniciar mininet,
mayor detalle en `readme` de carpeta  `redes2024_ob2/dist`

```
mininet> client cd dist/; python3.8 clientTest.py
```




