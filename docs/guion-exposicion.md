# Guion de exposición

Tiempos pensados para 10 minutos (semana 8) y 15 minutos (semana 16).

## 1. El problema real (1 min)

No es "buscar en una lista". Son 34 886 registros, 81 MB, 13.3 millones de
tokens, y hay que responder sub‑cadenas en milisegundos. Mostrar `head -3` del
CSV crudo: comillas escapadas, `\r\n` dentro del campo `Plot`, `[1]` de
Wikipedia, `Director = Unknown`, `Cast` vacío.

**Frase clave:** "El primer problema no fue el árbol, fue el CSV."

## 2. Pre‑procesamiento (2 min)

Mostrar la tabla del README §5 con los números de cada regla.
Recalcar que `std::getline` **rompe** este archivo porque 23 425 sinopsis
tienen saltos de línea incrustados.

## 3. La decisión de diseño (3 min) — el corazón de la nota

Secuencia a pizarra:

1. Queremos sub‑cadenas → suffix tree.
2. Suffix tree del texto completo → 1–1.5 GB. Inviable.
3. **Observación:** 13.3 M de tokens, pero solo 143 535 palabras distintas.
4. Indexamos el **vocabulario**, no el texto: 250× menos.
5. La equivalencia: `P` está en `D` ⟺ existe `T` que contiene `P` y `T` está en `D`.
6. Trie de sufijos del vocabulario + índice invertido.

Cifra para cerrar: **947 941 nodos, 135 MB, consultas en menos de 9 ms.**

Si preguntan por qué lista de hermanos y no arreglo de 36 hijos: arreglo = 190 MB
solo en punteros nulos; lista de hermanos = 21 MB, y con σ = 36 el recorrido es
constante en la práctica.

## 4. Demo en vivo (4 min)

Orden sugerido, sin improvisar:

```
ship                     -> palabra: salen barcos, no "relationships"
ghost ship               -> frase: las tres "Ghost Ship" primero
shi                      -> sub-palabra
bar                      -> sub-palabra del enunciado
director:hitchcock       -> tag
reparto:tom hanks        -> tag de dos palabras
genero:horror anio:1960  -> tags combinados
m                        -> siguientes cinco
2                        -> ficha: sinopsis, Like, Ver más tarde
l  v                     -> marcar
i                        -> inicio: Ver más tarde + recomendaciones
```

Señalar en pantalla la latencia en milisegundos que imprime cada búsqueda y la
explicación entre paréntesis de cada resultado.

## 5. El bug del IDF (2 min) — lo que diferencia la exposición

Contarlo como historia, con el antes y el después:

* **Antes:** buscar `ship` devolvía primero una película sin barcos, solo porque
  un actor se apellidaba **Shipp**.
* **Diagnóstico:** `shipp` aparece en 1 de 34 814 películas → IDF ≈ 10, contra
  IDF ≈ 2.9 de `ship`. El IDF se comía la penalización léxica.
* **Arreglo:** una coincidencia parcial no reclama el IDF de su término aislado;
  se acota por el IDF de la unión de todas las expansiones del patrón.
* **Después:** top‑5 = *Flying Phantom Ship*, *Sail a Crooked Ship*,
  *The Devil‑Ship Pirates*, *Ship Ahoy*, *Hell Ship Mutiny*.

Esto demuestra que el ranking se **midió**, no se adivinó.

## 5bis. Caché binario (1 min, si sobra tiempo)

Arranque de 4.89 s a 1.33 s serializando catálogo e índice. Mencionar la
validación por magia + versión + tamaño del CSV + `sizeof(Publicacion)`, y el
detalle de que `std::pair` no es trivialmente copiable en libstdc++, lo que
obligó a definir un POD propio. Es un buen ejemplo de "detalle del lenguaje que
solo aparece cuando escribes bytes crudos".

## 6. Recomendaciones (1 min)

Perfil = centroide tf‑idf de los Like; coseno contra los candidatos que comparten
términos. Demo: Like a *Destroy All Monsters* → salen cinco películas de Godzilla.

## 7. Cierre (1 min)

Tabla de complejidad, tabla de latencias, y las limitaciones conocidas del
README §11. Admitir las limitaciones suma: muestra que se conocen los límites del
diseño.

## Preguntas probables

| Pregunta | Respuesta corta |
|---|---|
| ¿Por qué no una tabla hash? | O(1) para palabra exacta, pero no resuelve prefijo ni sub‑cadena, y el enunciado pide un árbol de caracteres. |
| ¿Por qué BM25 y no TF‑IDF? | Saturación (evita que 20 repeticiones aplasten un título) y normalización por longitud (las sinopsis van de 40 a 36 773 caracteres). |
| ¿Y si el patrón tiene 1 letra? | Se resuelve solo la coincidencia exacta; expandir una letra devolvería casi todo el vocabulario y no aporta información. |
| ¿Cómo escalaría a 10× más datos? | El vocabulario crece sublinealmente (ley de Heaps), así que los árboles casi no cambian; lo que crece es el índice invertido, que se particiona o se comprime con codificación por diferencias. |
| ¿Por qué el trie de sufijos y no uno comprimido (Patricia/Ukkonen)? | Bajaría a ~2·n nodos, pero sobre el vocabulario el trie plano ya cabe en 135 MB y es mucho más simple de justificar y depurar. Está en las mejoras futuras. |
