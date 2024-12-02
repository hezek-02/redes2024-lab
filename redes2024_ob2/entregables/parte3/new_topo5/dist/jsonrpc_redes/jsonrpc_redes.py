import socket
import json
import threading

class ClientStubRPC:
    request_id_counter = 0  #! Atributo estático, a nivel de clase

    def __init__(self, address, port): #?constructor, (self define referencia (this->),convención, no es palabra reservada)
        self.address = address
        self.port = port
        self.sock = None #socket

    def __getattr__(self, nameProc): #?Se llama cuando una búsqueda de atributo no ha encontrado el atributo en los lugares habituales, (entiendo: invocar procedimientos y atributos sin definir)
        def procUndefined(*args, **kwargs): #?función anónima, (entiendo: función sin nombre), kwargs es un diccionario que contiene los argumentos de la función, nombreArg->valorArg
            return self.__call__(nameProc, *args, **kwargs) 
        return procUndefined  

    def __call__(self, nameProc,  *args, **kwargs): #?Se llama cuando se intenta llamar a una instancia de una clase, (entiendo: invocar a la clase y lo hace como una función)
        notify = kwargs.pop("notify", False) #?obtiene el valor de la clave "notify" del diccionario kwargs, si no existe, devuelve False
        if (kwargs): #?si kwargs no está vacío
            args = list(args) + [kwargs] #?convierte los argumentos en una lista y agrega el diccionario kwargs
        #print(args)
        if self.sock is None:  # Si no hay conexión, intenta reconectar
            self.connect()
        if notify:
            id = None
        else:
            id = ClientStubRPC.request_id_counter 
            ClientStubRPC.request_id_counter += 1
        try:
            request = {
                "jsonrpc": "2.0",
                "method": nameProc, #?An identifier established by the Client that MUST contain a String, Number, or NULL value if included. If it is not included it is assumed to be a notification. The value SHOULD normally not be Null [1] and Numbers SHOULD NOT contain fractional parts [2]
                "params": args,     #?The Server MUST reply with the same value in the Response object if included. This member is used to correlate the context between the two objects.
                "id": id            #?Un objeto Request que es una Notification significa la falta de interés del Cliente en el objeto Response correspondiente y, como tal, no es necesario devolver ningún objeto Response al cliente. El servidor NO DEBE responder a una notificación
            }

            #? Serializar la solicitud y enviarla
            request_json = json.dumps(request) #?convierte el objeto en una cadena JSON
        except Exception :
            self.close()
            error = "{code: -32700, message: Error de parseo}"
            raise Exception(error)
        try:
            self.sock.sendall(request_json.encode('utf-8')) #?envía la solicitud al servidor remoto en formato JSON
        except Exception :
            self.close()
            error = "{code: -32603, message: Error Interno}"
            raise Exception(error)
        #print(f"datos enviados: {request_json}")

        if  not notify:
            #! En una solicitud regular, recibir la respuesta
            try:
                response_json = self.sock.recv(4096).decode('utf-8') #?recibe la respuesta del servidor remoto, cada 4096 bytes fh
                response = json.loads(response_json) #?carga la cadena JSON en un objeto Python, en este caso un diccionario
            except Exception :
                self.close()
                error = "{code: -32603, message: Error Interno}"
                raise Exception(error)
        else:
            response = {"result": ""}
        self.close()

        #! Manejar errores
        if response.get("error") is not None:
            raise Exception(response.get("error"))
        return response.get("result")

    def close(self):
        if self.sock is not None:
            self.sock.close()
            self.sock = None

    def connect(self):
        if self.sock is not None:
            raise RuntimeError("Ya está conectado.") #?se dispara si se modifica literalmente el mismo objeto, en cliente es diferente, osea habria q modificar el self dos veces sin alterar el objeto, no pasaria nuna
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM) #?crea un socket de cliente TCP, AF_INET es la familia de direcciones, SOCK_STREAM es el tipo de socket para TCP 
        self.sock.connect((self.address, self.port)) #?conecta el socket al servidor remoto
        return self
    

class CortarEJEC(Exception):
    pass
class BaseServerRPC:
    methods = {}

    def __init__(self, address, port):
        self.address = address
        self.port = port
        self.server_socket = None

    def add_method(self, method):
        self.methods[method] = self.__getattribute__(method)

    def __errorMessage(self, code, message, id):
        return {
            "jsonrpc": "2.0",
            "error": #?formato mensaje de errord
                {
                    "code": code,
                    "message": message
                },
            "id": id
        }

    def handle_client(self, conn):
        """Maneja la comunicación con un cliente."""
        try:
            request = conn.recv(4096).decode('utf-8')
            if not request:
                return

            try:
                request_data = json.loads(request)
                proceso = request_data.get("method")
                arguments = request_data.get("params", [])
		
                id = request_data.get("id")
            except json.JSONDecodeError:
                response = self.__errorMessage(-32700, "Error de parseo", None)
                conn.sendall(json.dumps(response).encode('utf-8'))
                return

            if id is None:
                # Notificación
                print("\nNotificación: \n\t", proceso, ": ", arguments, "\n")
                return

            if proceso not in self.methods:
                response = self.__errorMessage(-32601, "Procedimiento no encontrado", id)
            else:
                try:
                    if (arguments is None):
                        resultado = self.methods[proceso]()
                    else:
                        resultado = self.methods[proceso](*arguments) #?llamada a la función
                    response = {
                        "jsonrpc": "2.0",
                        "result": resultado,
                        "id": id
                    }
                except TypeError as e:
                    print(e)
                    response = self.__errorMessage(-32602, "Parametros inválidosdas", id)
                except ValueError:
                    response = self.__errorMessage(-32600, "Solicitud Inválida", id)
                except Exception:
                    response = self.__errorMessage(-32603, "Error Interno", id)
            if id is not None:
                conn.sendall(json.dumps(response).encode('utf-8'))

        except Exception as e:
            print(f"Error manejando el cliente: {e}")
        finally:
            conn.close()

    def serve(self):
        """Inicia el servidor"""
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.bind((self.address, self.port))
        self.server_socket.listen(5)
        print(f"Servidor escuchando {self.address}:{self.port}")

        try:
            while True:
                try:
                    conn, addr = self.server_socket.accept()
                    print(f"Conexión establecida con {addr}")
                    # Crear un hilo para manejar la solicitud del cliente
                    client_thread = threading.Thread(target=self.handle_client, args=(conn,))
                    client_thread.start()
                except Exception as e:
                    print(f"Error aceptando la conexión: {e}")
        except CortarEJEC:
            self.close()


    def close(self):
        """Cerrar el servidor"""
        print("Cerrando servidor")
        self.server_socket.close()
        raise CortarEJEC("Se detuvo la ejecución")
    
    def detener_servidor(self):
        raise CortarEJEC("El servidor ha sido detenido.")
