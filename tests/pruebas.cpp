// pruebas.cpp — Pruebas unitarias y de integracion (sin dependencias externas).
#include <cstdlib>
#include <iostream>
#include <string>

#include "buscador.hpp"
#include "catalogo.hpp"
#include "csv.hpp"
#include "estado_usuario.hpp"
#include "indice.hpp"
#include "normalizador.hpp"
#include "recomendador.hpp"
#include "trie.hpp"

namespace {
int fallos = 0;
int total = 0;

void revisar(bool condicion, const std::string &nombre) {
  ++total;
  if (condicion) {
    std::cout << "  [ok]    " << nombre << "\n";
  } else {
    ++fallos;
    std::cout << "  [FALLA] " << nombre << "\n";
  }
}

template <typename T>
void igual(const T &obtenido, const T &esperado, const std::string &nombre) {
  ++total;
  if (obtenido == esperado) {
    std::cout << "  [ok]    " << nombre << "\n";
  } else {
    ++fallos;
    std::cout << "  [FALLA] " << nombre << " -> obtenido=" << obtenido
              << " esperado=" << esperado << "\n";
  }
}

// ---------------------------------------------------------------------------
void pruebasNormalizador() {
  std::cout << "\n== Normalizador ==\n";
  igual(pf::normalizar("El Capitán ÑOÑO"), std::string("el capitan nono"),
        "quita tildes y pasa a minusculas");
  igual(pf::normalizar("Wall-E 2008!"), std::string("wall e 2008 "),
        "los simbolos se vuelven espacios");
  igual(pf::normalizar("don't"), std::string("dont"),
        "el apostrofe no parte la palabra");
  igual(pf::limpiarTextoBruto("un barco.[12] Fin"), std::string("un barco. Fin"),
        "elimina referencias [12]");
  igual(pf::limpiarTextoBruto("linea uno\r\nlinea dos"),
        std::string("linea uno linea dos"), "elimina CRLF incrustados");
  const auto toks = pf::tokenizar("Le Bateau Fantôme (1968)");
  igual(toks.size(), size_t(4), "tokeniza 4 palabras");
  revisar(toks[2] == "fantome", "plegado de o con circunflejo");
  revisar(pf::esStopword("the") && !pf::esStopword("ship"), "stopwords");
}

// ---------------------------------------------------------------------------
void pruebasCsv() {
  std::cout << "\n== Lector CSV ==\n";
  pf::LectorCSV lector("tests/mini.csv");
  igual(lector.encabezados().size(), size_t(8), "8 columnas de encabezado");
  igual(lector.columna("Plot"), 7, "posicion de la columna Plot");
  std::vector<std::string> fila;
  revisar(lector.siguienteFila(fila), "lee la primera fila");
  igual(fila.size(), size_t(8), "la fila tiene 8 campos");
  igual(fila[1], std::string("Ghost Ship"), "campo Title");
  revisar(fila[4].find(',') != std::string::npos,
          "el campo entrecomillado con coma no se parte");
}

// ---------------------------------------------------------------------------
void pruebasTries() {
  std::cout << "\n== Tries ==\n";
  pf::TriePrefijos trie;
  const int idBarco = trie.insertar("barco");
  trie.insertar("barcaza");
  trie.insertar("bar");
  trie.insertar("cantina");
  igual(trie.insertar("barco"), idBarco, "insertar dos veces devuelve el mismo id");
  igual(trie.numTerminos(), size_t(4), "vocabulario de 4 terminos");
  igual(trie.buscar("barco"), idBarco, "busqueda exacta");
  igual(trie.buscar("barcos"), -1, "palabra inexistente");
  igual(trie.conPrefijo("bar", 100).size(), size_t(3), "3 palabras con prefijo bar");
  igual(trie.conPrefijo("zz", 100).size(), size_t(0), "prefijo sin resultados");

  pf::TrieSufijos sufijos;
  sufijos.insertarTermino("barco", 0);
  sufijos.insertarTermino("barcaza", 1);
  sufijos.insertarTermino("cantinabar", 2);
  sufijos.insertarTermino("nube", 3);
  igual(sufijos.terminosQueContienen("bar", 100).size(), size_t(3),
        "3 terminos contienen 'bar' (incluido al final de palabra)");
  igual(sufijos.terminosQueContienen("arc", 100).size(), size_t(2),
        "'arc' aparece en medio de dos terminos");
  igual(sufijos.terminosQueContienen("ube", 100).size(), size_t(1), "sufijo 'ube'");
  igual(sufijos.terminosQueContienen("xyz", 100).size(), size_t(0),
        "patron inexistente");
  igual(sufijos.terminosQueContienen("barco", 100)[0], 0,
        "la palabra completa se encuentra a si misma");
}

// ---------------------------------------------------------------------------
void pruebasCatalogoEIndice() {
  std::cout << "\n== Catalogo, indice y busqueda ==\n";
  pf::Catalogo catalogo;
  const pf::EstadisticasCarga ec = catalogo.cargarDesdeCSV("tests/mini.csv");
  igual(ec.filasLeidas, size_t(7), "7 filas de datos");
  igual(ec.descartadasDuplicadas, size_t(1), "descarta el duplicado exacto");
  igual(ec.descartadasSinopsisCorta, size_t(1), "descarta la sinopsis corta");
  igual(catalogo.tamano(), size_t(5), "quedan 5 peliculas utiles");

  // Reglas de limpieza.
  int idxBar = -1;
  for (size_t i = 0; i < catalogo.tamano(); ++i) {
    if (catalogo[i].tituloNorm == "the bar fight") idxBar = static_cast<int>(i);
  }
  revisar(idxBar >= 0, "encuentra The Bar Fight");
  revisar(catalogo[idxBar].director.empty(), "'Unknown' se traduce a director vacio");
  revisar(catalogo[idxBar].generos.empty(), "'unknown' se traduce a genero vacio");
  revisar(catalogo[0].sinopsis.find("[1]") == std::string::npos,
          "la sinopsis guardada no tiene referencias");

  pf::Indice indice;
  indice.construir(catalogo);
  revisar(indice.numDocumentos() == 5, "el indice tiene 5 documentos");
  revisar(indice.vocabulario().buscar("ship") >= 0, "'ship' esta en el vocabulario");

  pf::Buscador buscador(catalogo, indice);

  // 1) Palabra.
  auto r = buscador.buscar(buscador.interpretar("ship"), 10);
  revisar(!r.empty(), "buscar por palabra devuelve resultados");
  revisar(r.size() >= 3, "'ship' aparece en al menos 3 peliculas");

  // 2) Frase: semantica "y/o", pero las que tienen TODAS las palabras primero.
  r = buscador.buscar(buscador.interpretar("ghost ship"), 10);
  revisar(r.size() >= 3, "la frase devuelve tambien coincidencias parciales");
  revisar(catalogo[r[0].doc].tituloNorm == "ghost ship",
          "la coincidencia de frase completa va primero");

  // 3) Sub-palabra.
  r = buscador.buscar(buscador.interpretar("bar"), 10);
  bool hayBartender = false;
  for (const auto &x : r) {
    if (catalogo[x.doc].sinopsis.find("bartender") != std::string::npos)
      hayBartender = true;
  }
  revisar(hayBartender, "'bar' encuentra la pelicula que dice 'bartender'");

  // 4) Etiquetas.
  r = buscador.buscar(buscador.interpretar("director:sewell"), 10);
  revisar(r.size() == 1 && catalogo[r[0].doc].anio == 1952,
          "filtro por director");
  r = buscador.buscar(buscador.interpretar("genero:horror"), 10);
  revisar(r.size() == 1 && catalogo[r[0].doc].anio == 2002, "filtro por genero");
  r = buscador.buscar(buscador.interpretar("reparto:margulies"), 10);
  revisar(r.size() == 1, "filtro por reparto");
  r = buscador.buscar(buscador.interpretar("anio:1961"), 10);
  revisar(r.size() == 1, "filtro por anio");

  // 5) Acentos en la consulta y en los datos.
  r = buscador.buscar(buscador.interpretar("capitan"), 10);
  revisar(!r.empty(), "'capitan' encuentra 'capitán' (sin tilde en la consulta)");

  // 6) Consulta sin coincidencias.
  const pf::Consulta vacia = buscador.interpretar("qwertyasdf");
  revisar(!vacia.valida, "consulta sin coincidencias se marca invalida");

  // 7) Recomendaciones.
  pf::Recomendador recomendador(catalogo, indice);
  int docGhost1952 = -1;
  for (size_t i = 0; i < catalogo.tamano(); ++i) {
    if (catalogo[i].tituloNorm == "ghost ship" && catalogo[i].anio == 1952)
      docGhost1952 = static_cast<int>(i);
  }
  auto sim = recomendador.similaresA(docGhost1952, 3);
  revisar(!sim.empty(), "similaresA devuelve vecinos");
  revisar(!sim.empty() && sim[0].doc != docGhost1952,
          "no se recomienda a si misma");
  bool encontroLaOtraGhostShip = false;
  for (const auto &x : sim) {
    if (catalogo[x.doc].tituloNorm == "ghost ship") encontroLaOtraGhostShip = true;
  }
  revisar(encontroLaOtraGhostShip, "la otra Ghost Ship es la mas parecida");

  // 8) Estado del usuario y persistencia.
  pf::EstadoUsuario estado;
  revisar(estado.alternarMeGusta(docGhost1952), "alternar activa Me gusta");
  revisar(!estado.alternarMeGusta(docGhost1952), "alternar de nuevo lo desactiva");
  estado.alternarMeGusta(docGhost1952);
  estado.alternarVerMasTarde(0);
  revisar(estado.guardar("tests/estado_prueba.tmp", catalogo), "guarda el estado");
  pf::EstadoUsuario recargado;
  recargado.cargar("tests/estado_prueba.tmp", catalogo);
  igual(recargado.meGusta().size(), size_t(1), "recupera 1 Me gusta");
  igual(recargado.verMasTarde().size(), size_t(1), "recupera 1 Ver mas tarde");
  revisar(recargado.tieneMeGusta(docGhost1952), "el Me gusta apunta a la misma pelicula");
  std::remove("tests/estado_prueba.tmp");

  // 9) Cache binario: ida y vuelta.
  const std::string rutaCache = "tests/cache_prueba.tmp";
  revisar(indice.guardarCache(rutaCache, catalogo, 12345u), "escribe el cache");

  pf::Catalogo catalogo2;
  pf::Indice indice2;
  pf::EstadisticasIndice est2;
  revisar(indice2.cargarCache(rutaCache, catalogo2, 12345u, est2),
          "lee el cache");
  igual(catalogo2.tamano(), catalogo.tamano(), "mismo numero de peliculas");
  igual(catalogo2[0].titulo, catalogo[0].titulo, "mismo titulo tras el viaje");
  igual(catalogo2[0].sinopsis, catalogo[0].sinopsis, "misma sinopsis");
  igual(indice2.vocabulario().numTerminos(), indice.vocabulario().numTerminos(),
        "mismo vocabulario");

  pf::Buscador buscador2(catalogo2, indice2);
  auto r2 = buscador2.buscar(buscador2.interpretar("ghost ship"), 10);
  revisar(r2.size() == r.size() || !r2.empty(), "el cache permite buscar");
  auto rOrig = buscador.buscar(buscador.interpretar("ghost ship"), 10);
  bool mismoOrden = r2.size() == rOrig.size();
  for (size_t i = 0; mismoOrden && i < r2.size(); ++i) {
    if (catalogo2[r2[i].doc].tituloNorm != catalogo[rOrig[i].doc].tituloNorm)
      mismoOrden = false;
  }
  revisar(mismoOrden, "el ranking desde cache es identico al reconstruido");

  // La huella distinta invalida el cache.
  pf::Catalogo catalogo3;
  pf::Indice indice3;
  pf::EstadisticasIndice est3;
  revisar(!indice3.cargarCache(rutaCache, catalogo3, 99999u, est3),
          "una huella distinta invalida el cache");
  std::remove(rutaCache.c_str());
}

} // namespace

int main() {
  std::cout << "Pruebas del proyecto final de Programacion III\n";
  pruebasNormalizador();
  pruebasCsv();
  pruebasTries();
  pruebasCatalogoEIndice();
  std::cout << "\n----------------------------------------\n";
  std::cout << (total - fallos) << "/" << total << " pruebas pasaron.\n";
  return fallos == 0 ? 0 : 1;
}
