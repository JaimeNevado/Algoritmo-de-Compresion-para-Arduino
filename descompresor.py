import json
import re
import csv
import os

# Rutas (deben coincidir con el compresor)
NOMBRE_ARCHIVO = "CodigosComprimidos/arduinoZIP.ino"
ARCHIVO_SALIDA = "CodigosSalida/arduinoReestructurado.ino"

archivo = open("rosetta.json", "r", encoding="utf-8")
diccionarioFuncionesArduino = json.load(archivo)

# --- Funciones de I/O ---


def leerArchivo(nombreArchivo):
    try:
        with open(nombreArchivo, "r", encoding="utf-8") as archivo:
            return archivo.read()
    except FileNotFoundError:
        print(f"ERROR: El archivo de entrada no se encontró: {nombreArchivo}")
        return None


def escribirArchivo(nombreArchivo, contenido):
    try:
        os.makedirs(os.path.dirname(nombreArchivo), exist_ok=True)
        with open(nombreArchivo, "w", encoding="utf-8") as archivo:
            archivo.write(contenido)
        print(f"Archivo descomprimido guardado en: {nombreArchivo}")
    except IOError as e:
        print(f"No se pudo escribir el archivo de salida: {e}")


def reemplazar(texto, diccionario):
    textoFinal = texto
    for clave, valor in diccionario.items():
        textoFinal = textoFinal.replace(valor, clave)
    return textoFinal


def calcularLongitud(texto):
    totalCaracteres = 0
    for elemento in texto:
        totalCaracteres += 1
    return totalCaracteres


textoLeido = leerArchivo(NOMBRE_ARCHIVO)
textoFinal = reemplazar(textoLeido, diccionarioFuncionesArduino)


escribirArchivo(ARCHIVO_SALIDA, textoFinal)

print("Compresión terminada:")
