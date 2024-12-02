# Instructivo/Guía de pruebas a realizar

## Topología 5 routers

Se deben iterar las pruebas para `caída de vhost2`, `caída de vhost3` y `Recuperación de las situaciones anteriores`.

#### Traceroute

```
client traceroute -n server1
client traceroute -n server2
client traceroute -T -n  server1
client traceroute -T -n  server2
```

#### Ping

```
client ping server1
client ping server2
client ping -t 3 server2
client ping -c 3 10.0.2.2
client ping -c 3 10.0.2.1
client ping -c 3 200.0.0.50
client ping -c 3 200.200.0.2
```

#### Cliente-Servidor RPC 

```
client cd dist/; python3.8 clientTest.py
```

## Topología 3 routers

#### Traceroute

```
client traceroute -n server1
client traceroute -n server2
client traceroute -T -n  server1
client traceroute -T -n  server2
```

#### Ping

```
client ping -c 3 server1
client ping -c 3 server2
client ping -t 3 server2
client ping -c 3 10.0.2.2
client ping -c 3 10.0.2.1
client ping -c 3 200.0.0.50
```

#### Cliente-Servidor RPC 

```
client cd dist/; python3.8 test-client.py
```




