# Cómo subir el proyecto a GitHub

El enunciado exige: *"Subir el programa a un repositorio en Github. En el
repositorio debe de estar toda la documentación sobre el proyecto."*

## 1. Crear el repositorio

Uno del grupo crea el repositorio en GitHub (**público**, o privado invitando al
profesor y al JP como colaboradores). Sin `README` ni `.gitignore` automáticos:
este proyecto ya trae los suyos.

## 2. Primer push

Desde la carpeta del proyecto:

```bash
git init
git add .
git commit -m "Estructura del proyecto, lector CSV y pre-procesamiento"
git branch -M main
git remote add origin https://github.com/<usuario>/<repositorio>.git
git push -u origin main
```

Los otros integrantes clonan:

```bash
git clone https://github.com/<usuario>/<repositorio>.git
cd <repositorio>
# descargar el CSV en data/ (ver data/LEEME.md)
make -j && ./pruebas
```

## 3. Lo que NO se sube

`.gitignore` ya lo cubre, pero conviene entender por qué:

| Archivo | Por qué se excluye |
|---|---|
| `data/*.csv` | 81 MB. GitHub avisa a partir de 50 MB y rechaza a partir de 100 MB. Además `git` guardaría una copia completa por cada versión del archivo. |
| `*.cache` | 201 MB, y es un artefacto derivado: se regenera solo. |
| `build/`, `*.o`, `streaming`, `pruebas` | Binarios: se recompilan. |
| `datos_usuario.txt` | Estado local de cada integrante. |

Verifica antes del primer push:

```bash
git status --short          # no debe aparecer ningún .csv ni .cache
git count-objects -vH       # size-pack debería quedar por debajo de 1 MB
```

Si por accidente ya commiteaste el CSV, **no basta con borrarlo**: queda en el
historial. Hay que reescribirlo con `git filter-repo` o, si el repo es nuevo,
borrarlo y empezar de cero (más rápido).

## 4. Repartir los commits entre los cuatro

Varios profesores revisan el historial para ver quién trabajó. Un repositorio
con 40 commits de una sola persona es una señal de alarma, aunque el trabajo
haya sido grupal.

Trabajen en ramas por área y hagan merge con Pull Request:

```bash
git checkout -b feature/tries
# ... trabajar ...
git add include/trie.hpp src/trie.cpp
git commit -m "Trie de sufijos del vocabulario para búsqueda por sub-cadena"
git push -u origin feature/tries
# abrir el Pull Request en GitHub y que otro integrante lo revise
```

Reparto sugerido, alineado con la tabla de integrantes del README:

| Rama | Archivos |
|---|---|
| `feature/preprocesamiento` | `csv.*`, `catalogo.*`, `normalizador.*` |
| `feature/tries` | `trie.*`, `indice.*` |
| `feature/ranking` | `buscador.*`, `recomendador.*` |
| `feature/interfaz` | `main.cpp`, `estado_usuario.*`, `tests/` |

Que cada Pull Request lo apruebe otro integrante: eso deja evidencia de revisión
cruzada, que es justo lo que el profesor busca.

## 5. Etiquetar las dos entregas

El enunciado fija dos fechas: semana 8 (avance) y semana 16 (final).
Etiqueta cada una para que quede claro qué se presentó y cuándo:

```bash
git tag -a avance-semana8 -m "Entrega de avance: carga, tries, búsqueda básica"
git push origin avance-semana8
```

Y lo mismo con `entrega-final` en la semana 16. En la exposición puedes mostrar
el diff entre ambas etiquetas para evidenciar el progreso.

## 6. Antes de cada entrega

- [ ] La tabla de integrantes del README está completa (nombres y códigos).
- [ ] El texto del enunciado **no** está en el repositorio: el enunciado dice
      *"El siguiente texto debe ser eliminar en su repositorio"*.
- [ ] `git clone` en una máquina limpia + `make -j && ./pruebas` funciona.
- [ ] Las 64 pruebas pasan.
- [ ] `README.md` se ve bien renderizado en GitHub (tablas y acentos).
- [ ] Los commits están repartidos entre los integrantes.
