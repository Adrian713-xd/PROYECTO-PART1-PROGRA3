# Carpeta de datos

El dataset **no se versiona**: pesa 81 MB, GitHub rechaza archivos de más de 100 MB
y `git` no comprime bien un CSV de ese tamaño.

Descárgalo desde el enlace del enunciado y guárdalo aquí con este nombre exacto:

```
data/wiki_movie_plots_deduped.csv
```

Comprobación rápida (Linux/macOS):

```bash
wc -l data/wiki_movie_plots_deduped.csv   # ~34 887 líneas físicas
head -1 data/wiki_movie_plots_deduped.csv # Release Year,Title,Origin/Ethnicity,...
```

El programa acepta también una ruta distinta como primer argumento:

```bash
./streaming /otra/ruta/peliculas.csv
```
