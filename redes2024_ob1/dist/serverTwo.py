from serverModel import ServerOne, ServerTwo
import threading
import time
import sys

def run_server(server):
    server.serve()

if __name__ == "__main__":
    server2 = ServerTwo('200.100.0.15', 8089)

    # Crear hilos para ejecutar servidores en paralelo
    server2_thread = threading.Thread(target=server2.serve)

    server2_thread.daemon = True

    # Iniciar los hilos
    server2_thread.start()

    

    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:#Ctrl + C
        server2.close()
        sys.exit()
    