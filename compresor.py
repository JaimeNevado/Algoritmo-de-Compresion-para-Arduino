import json
import re
import csv
import os
import sys
import re


def leerArchivo(nombreArchivo):
    try:
        with open(nombreArchivo, "r", encoding="utf-8") as archivo:
            return archivo.read()
    except FileNotFoundError:
        print(f"ERROR: El archivo de entrada no se encontró: {nombreArchivo}")
        return None


def eliminar_comentarios_multilinea(texto):
    return re.sub(r"/\*.*?\*/", "", texto, flags=re.DOTALL)


def escribirArchivo(nombreArchivo, contenido):
    try:
        os.makedirs(os.path.dirname(nombreArchivo), exist_ok=True)
        with open(nombreArchivo, "w", encoding="utf-8") as archivo:
            archivo.write(contenido)
        print(f"Archivo descomprimido guardado en: {nombreArchivo}")
    except IOError as e:
        print(f"No se pudo escribir el archivo de salida: {e}")


def eliminarComentarios(texto):
    textoSinComentarios = ""
    dentroDeComentario = False
    for i in range(0, len(texto)):
        if texto[i] == "/" and texto[i + 1] == "/" and dentroDeComentario == False:
            dentroDeComentario = True
        elif dentroDeComentario == False and texto[i] != "\n":
            textoSinComentarios += texto[i]
        elif texto[i] == "\n":
            textoSinComentarios += texto[i]
            dentroDeComentario = False

    return eliminar_comentarios_multilinea(textoSinComentarios)


def reemplazar(texto, diccionario):
    textoFinal = texto
    for clave, valor in diccionario.items():
        textoFinal = textoFinal.replace(clave, valor)
    return textoFinal


def calcularLongitud(texto):
    totalCaracteres = 0
    for elemento in texto:
        totalCaracteres += 1
    return totalCaracteres


if len(sys.argv) > 1:
    # Han pasado el archivo
    rutaArchivo = "CodigosFuente/" + sys.argv[1] + ".ino"

    # Abrimos la Tabla rosetta de las traducciones :)
    f = open("rosetta.json", "r", encoding="utf-8")
    diccionarioFuncionesArduino = json.load(f)

    textoLeido = leerArchivo(rutaArchivo)
    totalCaracteresAntes = calcularLongitud(textoLeido)

    textoSinComentarios = eliminarComentarios(textoLeido)
    textoFinal = reemplazar(textoSinComentarios, diccionarioFuncionesArduino)
    totalCaracteresDespues = calcularLongitud(textoFinal)

    escribirArchivo("CodigosComprimidos/arduinoZIP.ino", textoFinal)

    print("Compresión terminada:")
    print("Caracteres antes: ", totalCaracteresAntes)
    print("Caracteres despues: ", totalCaracteresDespues)
    print(
        "Porcentaje comprimido en el Archivo",
        sys.argv[1].upper(),
        "es del:",
        100 - ((totalCaracteresDespues / totalCaracteresAntes) * 100),
        "%",
    )

else:
    print("Error, se debe indicar el archivo que hay que comprimir")
