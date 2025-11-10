# Algoritmo-de-Compresion-para-Arduino

Arduino-Mini-Compressor: Un Analizador de Compresión por Diccionario

Resumen del Proyecto

Este repositorio contiene el trabajo de investigación para la asignatura de Teoría de la Información y Codificación. El proyecto consiste en el diseño, implementación y análisis teórico de un algoritmo de compresión lossless (sin pérdidas) optimizado para código fuente de Arduino (.ino).

El algoritmo implementado es un compresor por diccionario estático (tokenizador). Su objetivo no es competir con algoritmos de propósito general como DEFLATE (ZIP), sino servir como un caso de estudio práctico para:

Aplicar conceptos de codificación económica.

Medir la entropía de una fuente de información especializada (código C++).

Analizar la eficiencia de un algoritmo "inventivo" comparándolo con los límites teóricos (Entropía de Shannon) y óptimos (Codificación de Huffman).

Concepto Fundamental del Algoritmo

El compresor (compresor.py) funciona bajo un principio de tokenización y sustitución por diccionario.

Limpieza: El script primero elimina todos los comentarios (// ... y /* ... */) del archivo .ino para reducir el ruido.

Sustitución Estática: Utiliza un diccionario estático (rosetta.json) que mapea palabras clave y funciones comunes de Arduino (tokens) a alias cortos de 2 o 3 caracteres (ej. "pinMode" $\rightarrow$ "$c").

Compresión: El script genera un nuevo archivo (.ino comprimido) donde todas las apariciones de esas palabras clave han sido reemplazadas por sus alias. Los tokens que no están en el diccionario (como nombres de variables, números o símbolos como ;) se dejan sin modificar.

Este método es una implementación de Codificación por Diccionario (visto en el Tema 4 de la asignatura), que explota la alta redundancia y predictibilidad de un lenguaje de programación.

Características Principales

Este proyecto se divide en dos componentes principales: el compresor en sí y un script de análisis teórico.

compresor.py (El Algoritmo)

Compresión por Diccionario: Comprime un archivo .ino usando el mapa de tokens definido en rosetta.json.

Limpieza de Código: Elimina automáticamente comentarios de una y múltiples líneas.

Manejo por CLI: Acepta el nombre del archivo a comprimir como un argumento de terminal.

calcular_metricas.py (El Análisis)

Análisis de Entropía ($H$): Implementa la Fórmula de Entropía de Shannon (Tema 3, p. 23) para calcular el límite teórico de compresión en bits/token.

Cálculo de Límite Óptimo ($L_{opt}$): Implementa el algoritmo de Codificación de Huffman (Tema 3, p. 51) para calcular la longitud media de palabra de un código estadístico perfecto para esta fuente.

Análisis del Algoritmo Propio ($L_{mio}$): Simula el coste en bits del algoritmo compresor.py, teniendo en cuenta tanto los tokens comprimidos (alias) como los no comprimidos (longitud original).

Análisis Comparativo: Demuestra empíricamente la superioridad de la codificación por tokens (bloques) frente a la codificación por caracteres (Tema 4, p. 3).

Estructura del Directorio

/
├── CodigosFuente/
│   └── cohete.ino       (Archivo de ejemplo para la "fuente emisora")
├── CodigosComprimidos/
│   └── arduinoZIP.ino   (Salida generada por el compresor)
├── compresor.py         (El script de compresión)
├── calcular_metricas.py   (El script de análisis teórico)
├── rosetta.json         (El diccionario estático de tokens)
└── README.md            (Este archivo)


Modo de Uso

El proyecto requiere Python 3.10 o superior.

1. Para Comprimir un Archivo

Ejecute el script compresor.py seguido del nombre del archivo (sin la extensión .ino) que se encuentra en CodigosFuente/.

python3.10 compresor.py cohete


Entrada: CodigosFuente/cohete.ino

Salida: CodigosComprimidos/arduinoZIP.ino

2. Para Realizar el Análisis Teórico

Ejecute el script calcular_metricas.py de la misma manera. El script analizará el archivo fuente especificado y mostrará los resultados de la investigación.

python3.10 calcular_metricas.py cohete


Salida (Consola): Un análisis detallado de la entropía y las longitudes medias de palabra.

Análisis Teórico y Resultados

El objetivo central de la investigación era validar la teoría de la asignatura usando el archivo cohete.ino como "fuente emisora". El script calcular_metricas.py nos permite comparar la eficiencia de nuestro algoritmo.

La teoría de la codificación económica (Tema 3, p. 56) establece la relación:
Entropía ($H$) $\le$ Longitud Óptima ($L_{opt}$) $\le$ Longitud de Cualquier Otro Código ($L_{mio}$)

Nuestros resultados empíricos para la fuente cohete.ino validan esta teoría:

--- ANÁLISIS A NIVEL DE TOKENS (Fuente: cohete.ino) ---
Total de Tokens: 1098
Tokens Únicos (m): 199
==========================================
1. LÍMITE TEÓRICO (ENTROPÍA, H):    6.1102 bits/token
2. LÍMITE ÓPTIMO (HUFFMAN, L_opt): 6.1466 bits/token
3. LONGITUD 'MI ALGORITMO' (L_mio): 21.9089 bits/token
==========================================
LÍMITE TEÓRICO DE COMPRESIÓN (Tokens): 838.63 bytes


Interpretación de los Resultados

Validación de la Codificación por Bloques (Tokens):
El análisis de tokens (límite de 838.63 bytes) es 3.1 veces más eficiente que un análisis por caracteres (límite de 2622.77 bytes). Esto demuestra que la hipótesis de tratar el código como "palabras" (tokens) es un método de compresión vastamente superior para esta fuente, tal como se estudia en el Tema 4.

Análisis de Eficiencia del Algoritmo ($L_{mio}$):
Nuestra longitud media (L_mio = 21.91) es significativamente mayor que la óptima de Huffman (L_opt = 6.15). Este análisis expone las carencias del algoritmo "inventivo":

Coste de Alias: El alias más corto (ej. "$c") tiene un coste de 16 bits (len("$c") * 8), mientras que Huffman puede asignar códigos de 1 a 3 bits a los tokens más frecuentes.

Diccionario Incompleto: El algoritmo falla al no comprimir los tokens más frecuentes del archivo (como ;, (, ), i, =), los cuales tienen un coste de 8 bits. Huffman les asigna un coste mínimo (ej. 1-2 bits), reduciendo drásticamente la longitud media.

Conclusiones y Trabajo Futuro

Conclusión 1: El proyecto demuestra con éxito el diseño y funcionamiento de un compresor por diccionario estático, aplicando los conceptos de la asignatura.

Conclusión 2: El análisis empírico ha validado la teoría de la información, calculando la entropía ($H$) como un límite teórico y la codificación de Huffman ($L_{opt}$) como el límite práctico para la fuente dada.

Trabajo Futuro: El análisis identifica dos áreas claras de mejora:

Optimización Estadística: Añadir los símbolos de puntuación más frecuentes (;, (, )) al rosetta.json reduciría drásticamente el L_mio.

Implementación de Diccionario Dinámico: Para manejar tokens desconocidos (como nombres de variables: ALTITUD, TEMPERATURA), el siguiente paso sería implementar un algoritmo LZW (Lempel-Ziv-Welch), como se describe en el Tema 4 (p. 63), que pueda aprender estos tokens sobre la marcha.
