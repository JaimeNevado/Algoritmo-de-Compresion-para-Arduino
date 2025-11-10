import re
import math
import heapq
import csv
import os
import json  # <-- Importante: añadido para leer el JSON
import sys  # <-- AÑADIDO: para leer argumentos de terminal
from collections import Counter

# --- CONFIGURACIÓN ---

# ¡ELIMINADO! El archivo de fuente se tomará de los argumentos
# ARCHIVO_FUENTE = "CodigosFuente/cohete.ino"

# ¡CAMBIADO! Apunta a tu nuevo diccionario
ROSETTA_JSON_ARCHIVO = "rosetta.json"

# --- FUNCIONES DE CÁLCULO ---


def calcular_entropia(contador, longitud_total):
    """Calcula la entropía de Shannon para una lista de datos."""
    entropia = 0.0
    for simbolo, cuenta in contador.items():
        if cuenta > 0:
            probabilidad = cuenta / longitud_total
            entropia -= probabilidad * math.log2(probabilidad)
    return entropia


# ¡¡FUNCIÓN CORREGIDA!!
def calcular_codigos_huffman(contador, longitud_total):
    """
    Implementa el algoritmo de Huffman para obtener la LONGITUD en bits de cada token.
    Devuelve un diccionario: {token: longitud_bits}
    """
    # 1. Crear el heap (cola de prioridad)
    # Formato: [probabilidad, [[token, bits], [token, bits], ...]]
    heap = []
    for token, cuenta in contador.items():
        if cuenta > 0:
            probabilidad = cuenta / longitud_total
            # ¡CORRECCIÓN! Envolver en una lista extra: [prob, [[token, bits]]]
            # Esto representa una lista de todos los nodos hoja en esta rama.
            heapq.heappush(heap, [probabilidad, [[token, 0]]])

    # 2. Construir el árbol
    while len(heap) > 1:
        # Sacar los dos nodos con menor probabilidad
        lo = heapq.heappop(heap)
        hi = heapq.heappop(heap)

        prob_lo, data_lo = lo  # data_lo es una lista de [token, bits]
        prob_hi, data_hi = hi  # data_hi es una lista de [token, bits]

        # ¡CORRECCIÓN! Iterar sobre la lista de datos directamente
        # e incrementar la longitud de bits para todos en esa rama.
        for item in data_lo:
            item[1] += 1  # item[1] es la longitud_bits
        for item in data_hi:
            item[1] += 1

        # ¡CORRECCIÓN! Combinar las listas de nodos hoja
        nueva_prob = prob_lo + prob_hi
        nuevos_datos = data_lo + data_hi

        heapq.heappush(heap, [nueva_prob, nuevos_datos])

    # 3. Extraer el diccionario de longitudes {token: bits}
    codigos_huffman = {}
    if heap:
        # ¡CORRECCIÓN! El heap[0] ahora es [prob, [[token1, bits1], [token2, bits2], ...]]
        # Accedemos a heap[0][1] (la lista de datos)
        for item in heap[0][1]:
            codigos_huffman[item[0]] = item[
                1
            ]  # item[0] es el token, item[1] es la longitud

    return codigos_huffman


# ¡FUNCIÓN REESCRITA!
def cargar_mapa_rosetta():
    """Carga el diccionario 'rosetta.json'."""
    try:
        with open(ROSETTA_JSON_ARCHIVO, "r", encoding="utf-8") as f:
            mapa_rosetta = json.load(f)
        return mapa_rosetta
    except FileNotFoundError:
        print(f"ERROR: No se encontró '{ROSETTA_JSON_ARCHIVO}'.")
        print("       El cálculo de L_mio no funcionará.")
        return {}
    except json.JSONDecodeError:
        print(f"ERROR: '{ROSETTA_JSON_ARCHIVO}' no es un JSON válido.")
        return {}


# ¡FUNCIÓN REESCRITA!
def calcular_longitud_mio(token, mapa_rosetta):
    """
    Calcula la longitud en BITS de un token según "Mi Algoritmo" (rosetta.json).
    Asume 8 bits por carácter.
    """
    # 1. ¿Está el token en el diccionario rosetta?
    # El script de métricas analiza por tokens (ej. "pinMode").
    # Comprobamos si ese token exacto está en tu mapa.
    if token in mapa_rosetta:
        # y_i = longitud del alias (ej. "$c") * 8 bits
        return len(mapa_rosetta[token]) * 8

    # 2. Si no, es un token "tal cual" (ej. ';', '(', 'TEMPERATURA', 'i')
    # Tu nuevo compresor no maneja 'TEMPERATURA', así que contamos su longitud original.
    # y_i = longitud del token original * 8 bits
    return len(token) * 8


def calcular_longitud_media(contador, longitud_total, funcion_longitud_bits):
    """
    Calcula la longitud media de palabra (Fórmula C de la guía).
    L = Sum(p_i * y_i)
    """
    longitud_media = 0.0
    for token, cuenta in contador.items():
        probabilidad = cuenta / longitud_total
        # y_i = longitud en bits del código para ese token
        bits = funcion_longitud_bits(token)
        longitud_media += probabilidad * bits
    return longitud_media


# --- ANÁLISIS PRINCIPAL ---
# --- ANÁLIS...
def analizar_nivel_caracteres(contenido):
    """Calcula la entropía a nivel de caracteres individuales."""
    print("--- ANÁLISIS A NIVEL DE CARACTERES ---")
    datos = list(contenido)
    if not datos:
        print("No hay datos de caracteres.")
        return

    longitud_total = len(datos)
    contador = Counter(datos)
    entropia_por_caracter = calcular_entropia(contador, longitud_total)
    bits_teoricos = entropia_por_caracter * longitud_total
    bytes_teoricos = bits_teoricos / 8

    print(f"Longitud total: {longitud_total} caracteres")
    print(f"Entropía (H): {entropia_por_caracter:.4f} bits/caracter")
    print(f"Límite Teórico de Compresión: {bytes_teoricos:.2f} bytes")
    print("------------------------------------------\n")


def analizar_nivel_tokens(contenido):
    """
    Calcula la entropía a nivel de "tokens" (palabras, números, símbolos).
    Esto es más similar a cómo funciona tu compresor.
    """
    print("--- ANÁLISIS A NIVEL DE TOKENS ---")

    # Mismo tokenizador que usaste antes (Paso 1 de la guía)
    tokens = re.findall(r"(\b\w+\b|[^\s\w])", contenido)

    if not tokens:
        print("No se encontraron tokens.")
        return

    longitud_total = len(tokens)
    # Frecuencias (Paso 1) y Probabilidades (Paso 2)
    contador = Counter(tokens)

    print(f"Total de Tokens: {longitud_total}")
    print(f"Tokens Únicos (m): {len(contador)}")
    print("==========================================")

    # --- CÁLCULO 1: ENTROPÍA (H) ---
    # (Paso 3 de la guía)
    entropia_por_token = calcular_entropia(contador, longitud_total)
    print(f"1. LÍMITE TEÓRICO (ENTROPÍA, H):   {entropia_por_token:.4f} bits/token")

    # --- CÁLCULO 2: HUFFMAN (L_opt) ---
    # (Paso 5 de la guía)
    codigos_huffman = calcular_codigos_huffman(contador, longitud_total)
    # Fallback: si un token no está (no debería pasar), usa su long original
    len_huffman = lambda token: codigos_huffman.get(token, len(token) * 8)
    longitud_optima_huffman = calcular_longitud_media(
        contador, longitud_total, len_huffman
    )
    print(
        f"2. LÍMITE ÓPTIMO (HUFFMAN, L_opt): {longitud_optima_huffman:.4f} bits/token"
    )

    # --- CÁLCULO 3: MI ALGORITMO (L_mio) ---
    # (Paso 4 de la guía)

    # ¡CAMBIADO! Cargar el mapa rosetta
    mapa_rosetta_cargado = cargar_mapa_rosetta()

    # ¡CAMBIADO! La lambda ahora usa el nuevo mapa y función
    len_mio = lambda token: calcular_longitud_mio(token, mapa_rosetta_cargado)

    longitud_mi_algoritmo = calcular_longitud_media(contador, longitud_total, len_mio)
    print(f"3. LONGITUD 'MI ALGORITMO' (L_mio): {longitud_mi_algoritmo:.4f} bits/token")

    print("==========================================")

    # --- CÁLCULO 4: TAMAÑO TEÓRICO EN BYTES ---
    bits_teoricos = entropia_por_token * longitud_total
    bytes_teoricos = bits_teoricos / 8
    print(f"LÍMITE TEÓRICO DE COMPRESIÓN (Tokens): {bytes_teoricos:.2f} bytes")
    print("------------------------------------------\n")


# --- CÓDIGO PRINCIPAL DE EJECUCIÓN ---

if len(sys.argv) > 1:
    nombre_archivo_sin_extension = sys.argv[1]
    archivo_fuente = f"CodigosFuente/{nombre_archivo_sin_extension}.ino"

    print(f"Analizando la entropía y métricas de: {archivo_fuente}")
    print("==================================================\n")

    try:
        with open(archivo_fuente, "r", encoding="utf-8") as archivo:
            contenido_original = archivo.read()

        # Ejecutar ambos análisis
        analizar_nivel_tokens(contenido_original)
        # analizar_nivel_caracteres(contenido_original)

    except FileNotFoundError:
        print(f"ERROR: No se pudo encontrar el archivo de entrada: {archivo_fuente}")
        print("Asegúrate de que la ruta es correcta.")
    except Exception as e:
        print(f"Ocurrió un error: {e}")

else:
    print("Error: Debes indicar el nombre del archivo (sin .ino) a analizar.")
    print("Ejemplo: python3.10 calcular_metricas.py cohete")
