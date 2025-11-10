# Algoritmo de Compresion para Arduino

Este repositorio contiene el trabajo de investigación para la asignatura de Teoría de la Información y Codificación. El proyecto consiste en el diseño, implementación y análisis teórico de un algoritmo de compresión optimizado para código fuente de Arduino (.ino).

El algoritmo implementado es un compresor por diccionario estático (tokenizador). Su objetivo no es competir con algoritmos de propósito general como DEFLATE (ZIP), sino servir como un caso de estudio práctico para:

  - Aplicar conceptos de codificación económica.

  - Medir la entropía de una fuente de información especializada (código C++).

  - Analizar la eficiencia de un algoritmo "inventivo" comparándolo con los límites teóricos (Entropía de Shannon) y óptimos (Codificación de Huffman).

## El compresor (compresor.py) funciona bajo un principio de tokenización y sustitución por diccionario.

  - Limpieza: El script primero elimina todos los comentarios (// ... y /* ... */) del archivo .ino para reducir el ruido.

  - Sustitución Estática: Utiliza un diccionario estático (rosetta.json) que mapea palabras clave y funciones comunes de Arduino (tokens) a alias cortos de 2 o 3 caracteres (ej.             "pinMode" $\rightarrow$ "$c").

  - Compresión: El script genera un nuevo archivo (.ino comprimido) donde todas las apariciones de esas palabras clave han sido reemplazadas por sus alias. Los tokens que no están en         el diccionario (como nombres de variables, números o símbolos como ;) se dejan sin modificar.


## Modo de Uso

El proyecto requiere Python 3.10 o superior.

  1. Para Comprimir un Archivo
  
    Ejecute el script compresor.py seguido del nombre del archivo (sin la extensión .ino) que se encuentra en CodigosFuente/.
    
    (Ej: python3.10 compresor.py cohete)


  2. Para Realizar el Análisis Teórico
  
    Ejecute el script calcular_metricas.py de la misma manera. El script analizará el archivo fuente especificado y mostrará los resultados de la investigación.
    
    python3.10 calcular_metricas.py cohete

  3. Para Descomprimir el archivo comprimido
  
    Ejecute el script descomprimir.py de la misma manera. El archivo descomprimido se guardará en la carpeta CodigosSalida con el nombre arduinoReestructurado.ino
    
    python3.10 descompresor.py cohete


## Análisis Teórico y Resultados

  El objetivo central de la investigación era validar la teoría de la asignatura usando el archivo cohete.ino como "fuente emisora". El         script calcular_metricas.py nos permite comparar la eficiencia de nuestro algoritmo.

  La teoría de la codificación económica establece la relación:
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

## La explicación del proyecto se encuentra online mediante el siguiente enlace:
https://www.youtube.com/watch?v=1uZ4k4dRS4Q
