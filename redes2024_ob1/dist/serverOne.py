from serverModel import ServerOne
from jsonrpc_redes import Server
import threading
import time
import sys

def run_server(server):
    try:
        server.serve()
    except Exception as e:
        print(f"Error al ejecutar el servidor: {e}")

if __name__ == "__main__":
    try:
        # Crear el primer servidor (ServerOne) en el puerto 8087
        server1 = ServerOne('200.0.0.10', 8087)
        server1_thread = threading.Thread(target=run_server, args=(server1,))
        server1_thread.daemon = True
        server1_thread.start()

        # Definir funciones para el segundo servidor
        host, port = '200.0.0.10', 8080
    
        def echo(message):
            return message
            
        def summation(*args):
            return sum(args)

        def echo_concat(msg1, msg2, msg3, msg4):
            return msg1 + msg2 + msg3 + msg4
            
        server = Server((host, port))
        server.add_method(echo)
        server.add_method(summation, 'sum')
        server.add_method(echo_concat)
        server_thread = threading.Thread(target=server.serve)
        server_thread.daemon = True
        server_thread.start()

        print("Servidor 1 ejecutando en el puerto 8087")
        print("Servidor 2 ejecutando en el puerto 8080")

        # Bucle principal para mantener ambos servidores activos
        try:
            while True:
                time.sleep(0.5)
        except KeyboardInterrupt:
            print("Cerrando servidores...")
            server1.close()
            server.close()  # Cerrar ambos servidores
            sys.exit()

    except Exception as e:
        print(f"Error general: {e}")
        sys.exit(1)
