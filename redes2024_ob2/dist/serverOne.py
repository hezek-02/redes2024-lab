from serverModel import ServerOne
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
        server1 = ServerOne('150.150.0.2', 8087)
        server1_thread = threading.Thread(target=run_server, args=(server1,))
        server1_thread.daemon = True
        server1_thread.start()


        print("Servidor 1 ejecutando en el puerto 8087")

        # Bucle principal para mantener ambos servidores activos
        try:
            while True:
                time.sleep(0.5)
        except KeyboardInterrupt:
            print("Cerrando servidor...")
            server1.close()
            sys.exit()

    except Exception as e:
        print(f"Error general: {e}")
        sys.exit(1)
