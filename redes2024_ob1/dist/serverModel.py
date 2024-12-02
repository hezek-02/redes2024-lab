from jsonrpc_redes.jsonrpc_redes import BaseServerRPC

"""
funcionalidades mínimas:
• Obtener un objeto servidor: server = Server(('address', 
‘port’))
• Agregar un procedimiento: server.add_method(proc1)
• Ejecutar el servidor: server.serve() 
"""
class ServerOne(BaseServerRPC):
    def __init__(self, address, port):
        super().__init__(address, port)
        self.add_method("proc1")
        self.add_method("suma")
        self.add_method("multiplicacion")
        self.add_method("detener_servidor")
        self.add_method("metodo_con_args_opcionales")

    def proc1(self, x, y, **kwargs):
        return x
    
    def metodo_con_args_opcionales(self, param1, param2="valor por defecto", param3="", **kwargs):
        result = [param1, param2, param3]
        if kwargs:
            result = [param1] + list(kwargs.values())
        return result
    
    def suma(self, x, y):
        if not isinstance(x, (int, float)) or not isinstance(y, (int, float)):
            raise ValueError("Invalid request")
        return x + y
    
    def multiplicacion(self, x, y):
        if not isinstance(x, (int, float)) or not isinstance(y, (int, float)):
            raise ValueError("Invalid request")
        return x * y
    

class ServerTwo(BaseServerRPC):
    def __init__(self, address, port):
        super().__init__(address, port)
        self.add_method("proc2")
        self.add_method("resta")
        self.add_method("division")
        self.add_method("obtenerCien")
        self.add_method("obtenerUnidades")
        self.add_method("detener_servidor")
    
    def proc2(self, x, y):
        return y

    def resta(self, x, y):
        if not isinstance(x, (int, float)) or not isinstance(y, (int, float)):
            raise ValueError("Invalid request")
        return x - y
    
    def division(self, x, y):
        if not isinstance(x, (int, float)) or not isinstance(y, (int, float)):
            raise ValueError("Invalid request")
        return x / y
    def obtenerUnidades(self):
        return [0,1,2,3,4,5,6,7,8,9]
    
    def obtenerCien(self):
        return 100
