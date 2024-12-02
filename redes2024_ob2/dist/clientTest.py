from jsonrpc_redes.jsonrpc_redes import ClientStubRPC


'''
Cliente, 
•única función pública conn = connect(‘address’, ‘port’) la cual devuelve un elemento que representa
la conexión con el servidor remoto.

•Se utilizará de la siguiente forma para llamar al
procedimiento remoto proc1(arg1, arg2):
•result = conn.proc1(arg1, arg2)

•La implementación deberá permitir el envío de “notificaciones” tal cual se define
en la especificación. Para esto se sugiere el siguiente formato:
conn.proc1(arg1, arg2, notify=True)

•Para implementar la funcionalidad de que se pueda llamar a un procedimiento no
definido previamente en la clase Client recomendamos evaluar las siguientes
funciones de Python:
__getattr__ [9] y __call__ [10]
'''

def connect(address, port):
    client = ClientStubRPC(address, port)
    return client.connect()

def run_tests():
    try :
        conn1 = connect('150.150.0.2', 8087) #crea una instancia de la clase StubRPC
        conn2 = connect('100.100.0.2', 8089) #crea una instancia de la clase StubRPC
    except Exception as e:
        print("Error: ", e)

    try:
        print('Iniciando pruebas de casos sin errores.')

        assert conn1.suma(54, 22) == 76
        print("prueba de test simple completada")
        assert conn1.multiplicacion(13, 23) == 299
        print("prueba de test simple completada")
        assert conn2.proc2(14, 22) == 22
        print("prueba de test simple completada")
        assert conn2.resta(112, 22) == 90
        print("prueba de test simple completada")
        assert conn2.division(140, 20) == 7
        print("prueba de test simple completada")


        assert conn2.Msg("Hello W0rld", notify=True) == ""
        print("prueba de test notificacion completada")

        assert conn1.FantasticConecctionYOUYOU(
            "Hello W0rld and BAy", 
            "Other argumento for the mesaje", 
            "and another one SDSadr4", 
            "FANTASTIC 4 FORCE", 
            notify=True
        ) == ""
        print("prueba de test notificacion completada")
        
        assert conn2.proc2(1, 245, notify=True) == ""
        print("prueba de test notificacion completada")

        assert conn2.resta(1, 2, notify=True) == ""
        print("prueba de test notificacion completada")
        
        assert conn1.metodo_con_args_opcionales("obligatorio") == ["obligatorio", "valor por defecto", ""]
        print("prueba de test metodo params opcionales completada")

        assert conn1.metodo_con_args_opcionales("obligatorio", "opcional2") == ["obligatorio", "opcional2", ""]
        print("prueba de test metodo params opcionales completada")

        assert conn1.metodo_con_args_opcionales("obligatorio", param2="opcional2", param3="opcional3") == ['obligatorio', {'param2': 'opcional2', 'param3': 'opcional3'}, '']
        print("prueba de test metodo params opcionales completada")

        assert conn2.obtenerUnidades() == [0,1,2,3,4,5,6,7,8,9]
        print("prueba de test metodo que retorna lista completada")

        assert conn2.obtenerCien() == 100
        print("prueba de test metodo sin parametros completada")


        print('=============================\n')
        print('ÉXITO: Pruebas de casos sin errores completadas.')
        
        # Pruebas con errores
        print('=============================\n')
        print('Iniciando pruebas de casos con errores.')
        try:
            conn1.proc3(1,2)  #{'code': -32601, 'message': 'Procedimiento no encontrado'}
        except Exception as e:
            print("Lanzó excepcion, metodo sin definir: ", e)
        else:
            print('ERROR: No lanzó excepción.')
            return
        #!
        try:
            datosQueHaranFallarJASON = {
                "funcy": lambda x: x + 1,
                "set": {1, 2, 3},
                "src": "datos en formato not JSON",
            }
            conn1.suma(34, datosQueHaranFallarJASON)  #{'code': -32700, 'message': 'Error parse'}
        except Exception as e:
            print("Lanzó excepcion, Error de parseo : ", e)
        else:
            print('ERROR: No lanzó excepción.')
            return
        #!
        try:
            conn2.division(34, 22,312,432,5534)  #{'code': -32602, 'message': 'Parametros inválidos'}
        except Exception as e:
            print("Lanzó excepcion, Parametros inválidos: ", e)
        else:
            print('ERROR: No lanzó excepción.')
            return
        #!
        try:
            conn2.division(34, "22")  #{'code': -32600, 'message': 'Invalid Request'}
        except Exception as e:
            print("Lanzó excepcion, Solicitud Inválida: ", e)
        else:
            print('ERROR: No lanzó excepción.')
            return
        #!
        try:
            conn2.division(34, 0)  #{'code': -32603, 'message': 'Error Interno'}
        except Exception as e:
            print("Lanzó excepcion, Error Interno: ", e)
        else:
            print('ERROR: No lanzó excepción.')
            return
        

        print('=============================\n')
        print('ÉXITO: Pruebas de casos con errores completadas.')

        print('=============================\n')

        print('ÉXITO: Todas las pruebas completadas.')

    except Exception as e:
        print("ERROR durante la ejecución de pruebas:", e)

if __name__ == "__main__": #si el script se ejecuta directamente, es decir si python3 client.py
    run_tests()
        
"""
DONE:
-32600	Invalid Request	The JSON sent is not a valid Request object. DONE (SV)

-32700	Parse error	Invalid JSON was received by the server. DONE
An error occurred on the server while parsing the JSON text. (CLIENTE)

-32601	Method not found	The method does not exist / is not available. DONE (SV)

-32602	Invalid params	Invalid method parameter(s). DONE (SV)

-32603	Internal error	Internal JSON-RPC error. DONE (SV)

"""