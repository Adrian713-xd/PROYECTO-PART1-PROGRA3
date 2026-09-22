// main.cpp — Interfaz de consola de la plataforma de streaming.
//
// Programacion III (CS2013) — Proyecto Final 2026-2 — UTEC
#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "buscador.hpp"
#include "catalogo.hpp"
#include "estado_usuario.hpp"
#include "indice.hpp"
#include "normalizador.hpp"
#include "recomendador.hpp"

namespace {

constexpr size_t kPagina = 5; // el enunciado pide de cinco en cinco
const char *kRutaEstadoPorDefecto = "datos_usuario.txt";

std::string recortarAncho(const std::string &s, size_t ancho) {
  if (s.size() <= ancho) return s;
  return s.substr(0, ancho - 3) + "...";
}

// Busca 'patron' en 'texto' sin distinguir mayusculas (comparacion ASCII) y
// devuelve un fragmento centrado en la primera aparicion. Se trabaja sobre el
// texto ORIGINAL para no tener que mapear posiciones del texto normalizado.
std::string fragmento(const std::string &texto, const std::string &patron,
                      size_t ancho = 96) {
  if (patron.empty() || texto.empty()) return std::string();
  size_t donde = std::string::npos;
  for (size_t i = 0; i + patron.size() <= texto.size(); ++i) {
    size_t j = 0;
    while (j < patron.size() &&
           std::tolower(static_cast<unsigned char>(texto[i + j])) ==
               std::tolower(static_cast<unsigned char>(patron[j]))) {
      ++j;
    }
    if (j == patron.size()) { donde = i; break; }
  }
  if (donde == std::string::npos) return std::string();

  size_t inicio = donde > ancho / 3 ? donde - ancho / 3 : 0;
  while (inicio > 0 && texto[inicio] != ' ') --inicio; // no cortar palabras
  size_t fin = std::min(texto.size(), inicio + ancho);
  while (fin < texto.size() && texto[fin] != ' ') ++fin;

  std::string s;
  if (inicio > 0) s += "...";
  s += texto.substr(inicio, fin - inicio);
  if (fin < texto.size()) s += "...";
  for (char &c : s) {
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
  }
  return s;
}

void linea(char c = '-', size_t n = 78) {
  std::cout << std::string(n, c) << "\n";
}

std::string unir(const std::vector<std::string> &v, const std::string &sep,
                 size_t maximo = 6) {
  std::string s;
  for (size_t i = 0; i < v.size() && i < maximo; ++i) {
    if (i) s += sep;
    s += v[i];
  }
  if (v.size() > maximo) s += sep + "(+" + std::to_string(v.size() - maximo) + ")";
  return s;
}

class Aplicacion {
public:
  Aplicacion(pf::Catalogo &catalogo, pf::Indice &indice, std::string rutaEstado)
      : catalogo_(catalogo), indice_(indice), buscador_(catalogo, indice),
        recomendador_(catalogo, indice), rutaEstado_(std::move(rutaEstado)) {
    estado_.cargar(rutaEstado_, catalogo_);
  }

  void ejecutar() {
    mostrarInicio();
    std::string linea;
    while (true) {
      std::cout << "\n> ";
      if (!std::getline(std::cin, linea)) break;
      linea = pf::recortar(linea);
      if (linea.empty()) continue;
      if (linea == "q" || linea == "salir") break;
      if (!procesar(linea)) break;
    }
    estado_.guardar(rutaEstado_, catalogo_);
    std::cout << "\nEstado guardado en " << rutaEstado_ << ". Hasta luego.\n";
  }

private:
  pf::Catalogo &catalogo_;
  pf::Indice &indice_;
  pf::Buscador buscador_;
  pf::Recomendador recomendador_;
  pf::EstadoUsuario estado_;
  std::string rutaEstado_;

  std::vector<pf::Resultado> resultados_;
  size_t mostrados_ = 0;
  std::string consultaActual_;
  std::string terminoDestacado_; // patron con el que se recorta el fragmento

  // -------------------------------------------------------------------------
  void mostrarFicha(const pf::Resultado &r, size_t numero) {
    const pf::Pelicula &p = catalogo_[r.doc];
    std::cout << "  " << numero << ") " << recortarAncho(p.titulo, 52);
    if (p.anio > 0) std::cout << " (" << p.anio << ")";
    if (estado_.tieneMeGusta(r.doc)) std::cout << " [<3]";
    if (estado_.tieneVerMasTarde(r.doc)) std::cout << " [+]";
    std::cout << "\n";
    std::cout << "     ";
    if (!p.director.empty()) std::cout << "dir. " << recortarAncho(p.director, 34) << " | ";
    if (!p.generos.empty()) std::cout << unir(p.generos, ", ", 3) << " | ";
    if (r.puntaje > 0.0) {
      std::cout << "score " << std::fixed << std::setprecision(2) << r.puntaje
                << std::defaultfloat << " ";
    }
    if (!r.explicacion.empty()) std::cout << "(" << r.explicacion << ")";
    std::cout << "\n";
    if (!terminoDestacado_.empty()) {
      const std::string frag = fragmento(p.sinopsis, terminoDestacado_);
      if (!frag.empty()) std::cout << "     \"" << frag << "\"\n";
    }
  }

  void mostrarPagina() {
    if (resultados_.empty()) {
      std::cout << "Sin coincidencias.\n";
      return;
    }
    const size_t fin = std::min(mostrados_ + kPagina, resultados_.size());
    for (size_t i = mostrados_; i < fin; ++i) {
      mostrarFicha(resultados_[i], i + 1);
    }
    mostrados_ = fin;
    std::cout << "\n  Mostrando " << mostrados_ << " de " << resultados_.size();
    if (mostrados_ < resultados_.size()) {
      std::cout << "  ->  escribe 'm' para ver las siguientes " << kPagina;
    }
    std::cout << "\n  Escribe el numero de una pelicula para abrir su ficha.\n";
  }

  void mostrarDetalle(int doc) {
    const pf::Pelicula &p = catalogo_[doc];
    estado_.marcarVisto(doc);
    std::cout << "\n";
    linea('=');
    std::cout << p.titulo;
    if (p.anio > 0) std::cout << "  (" << p.anio << ")";
    std::cout << "\n";
    linea('=');
    if (!p.director.empty()) std::cout << "Director : " << p.director << "\n";
    if (!p.reparto.empty()) std::cout << "Reparto  : " << unir(p.reparto, ", ", 8) << "\n";
    if (!p.generos.empty()) std::cout << "Genero   : " << unir(p.generos, ", ") << "\n";
    if (!p.origen.empty()) std::cout << "Origen   : " << p.origen << "\n";
    if (!p.url.empty()) std::cout << "Wiki     : " << p.url << "\n";
    std::cout << "\nSINOPSIS\n";
    // Ajuste de linea a 78 columnas.
    std::istringstream iss(p.sinopsis);
    std::string palabra;
    size_t col = 0;
    while (iss >> palabra) {
      if (col + palabra.size() + 1 > 78) {
        std::cout << "\n";
        col = 0;
      }
      std::cout << palabra << " ";
      col += palabra.size() + 1;
    }
    std::cout << "\n\n";
    std::cout << (estado_.tieneMeGusta(doc) ? "[l] Quitar Me gusta   "
                                            : "[l] Me gusta          ");
    std::cout << (estado_.tieneVerMasTarde(doc) ? "[v] Quitar de Ver mas tarde   "
                                                : "[v] Ver mas tarde             ");
    std::cout << "[s] Similares   [Enter] volver\n";

    while (true) {
      std::cout << "ficha> ";
      std::string cmd;
      if (!std::getline(std::cin, cmd)) return;
      cmd = pf::recortar(cmd);
      if (cmd.empty()) return;
      if (cmd == "l") {
        const bool ahora = estado_.alternarMeGusta(doc);
        std::cout << (ahora ? "  Agregada a Me gusta.\n" : "  Quitada de Me gusta.\n");
        estado_.guardar(rutaEstado_, catalogo_);
      } else if (cmd == "v") {
        const bool ahora = estado_.alternarVerMasTarde(doc);
        std::cout << (ahora ? "  Agregada a Ver mas tarde.\n"
                            : "  Quitada de Ver mas tarde.\n");
        estado_.guardar(rutaEstado_, catalogo_);
      } else if (cmd == "s") {
        auto sim = recomendador_.similaresA(doc, 10);
        resultados_ = sim;
        terminoDestacado_.clear();
        mostrados_ = 0;
        std::cout << "\nSimilares a \"" << p.titulo << "\":\n";
        mostrarPagina();
        return;
      } else {
        return;
      }
    }
  }

  void mostrarInicio() {
    std::cout << "\n";
    linea('=');
    std::cout << "  UTEC STREAM  |  " << catalogo_.tamano()
              << " peliculas indexadas\n";
    linea('=');

    const auto &ver = estado_.verMasTarde();
    std::cout << "\nVER MAS TARDE (" << ver.size() << ")\n";
    if (ver.empty()) {
      std::cout << "  (vacio) Abre una pelicula y pulsa [v] para agregarla.\n";
    } else {
      resultados_.clear();
      terminoDestacado_.clear();
      for (size_t i = 0; i < ver.size(); ++i) {
        resultados_.push_back({ver[i], 0.0, "en tu lista"});
      }
      mostrados_ = 0;
      mostrarPagina();
    }

    const auto &likes = estado_.meGusta();
    std::cout << "\nRECOMENDADAS PARA TI\n";
    if (likes.empty()) {
      std::cout << "  (vacio) Da [l] Me gusta a alguna pelicula y aqui\n"
                   "  apareceran titulos parecidos.\n";
    } else {
      auto rec = recomendador_.paraUsuario(likes, estado_.verMasTarde(), 10);
      if (rec.empty()) {
        std::cout << "  Sin recomendaciones todavia.\n";
      } else {
        for (size_t i = 0; i < rec.size() && i < kPagina; ++i) {
          mostrarFicha(rec[i], i + 1);
        }
        recomendaciones_ = rec;
      }
    }
    std::cout << "\n";
    linea();
    ayuda();
  }

  std::vector<pf::Resultado> recomendaciones_;

  void ayuda() {
    std::cout <<
        "COMANDOS\n"
        "  <texto>              buscar por palabra, frase o sub-palabra\n"
        "                       ej: barco    |  barco fantasma  |  bar\n"
        "  director:<nombre>    buscar por etiqueta. Tambien: reparto:, genero:,\n"
        "                       origen:, titulo:, anio:\n"
        "                       ej: director:hitchcock genero:horror anio:1960\n"
        "  m                    ver las siguientes 5 coincidencias\n"
        "  <numero>             abrir la ficha de ese resultado\n"
        "  r                    abrir mis recomendaciones\n"
        "  i                    volver al inicio\n"
        "  a                    mostrar esta ayuda\n"
        "  q                    salir\n";
  }

  bool procesar(const std::string &entrada) {
    if (entrada == "a" || entrada == "ayuda") { ayuda(); return true; }
    if (entrada == "i" || entrada == "inicio") { mostrarInicio(); return true; }
    if (entrada == "m" || entrada == "mas") {
      if (mostrados_ >= resultados_.size()) {
        std::cout << "No hay mas coincidencias.\n";
      } else {
        mostrarPagina();
      }
      return true;
    }
    if (entrada == "r") {
      if (recomendaciones_.empty()) {
        std::cout << "Aun no hay recomendaciones. Marca algun Me gusta.\n";
      } else {
        resultados_ = recomendaciones_;
        terminoDestacado_.clear();
        mostrados_ = 0;
        mostrarPagina();
      }
      return true;
    }
    // Numero: abrir la ficha correspondiente.
    if (entrada.find_first_not_of("0123456789") == std::string::npos) {
      const size_t n = std::stoul(entrada);
      if (n >= 1 && n <= resultados_.size()) {
        mostrarDetalle(resultados_[n - 1].doc);
      } else {
        std::cout << "Numero fuera de rango.\n";
      }
      return true;
    }

    // Todo lo demas es una consulta de busqueda.
    consultaActual_ = entrada;
    const auto t0 = std::chrono::steady_clock::now();
    const pf::Consulta q = buscador_.interpretar(entrada);
    if (!q.valida) {
      std::cout << q.mensaje << "\n";
      return true;
    }
    terminoDestacado_ = q.terminos.empty() ? std::string() : q.terminos.front().patron;
    resultados_ = buscador_.buscar(q, 100);
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
            .count();
    mostrados_ = 0;
    std::cout << "\nResultados para \"" << entrada << "\"  ("
              << std::fixed << std::setprecision(1) << ms << " ms)"
              << std::defaultfloat << "\n";
    mostrarPagina();
    return true;
  }
};

} // namespace

namespace {

// Huella del CSV para validar el cache: tamano en bytes + fecha de modificacion.
uint64_t huellaDeArchivo(const std::string &ruta) {
  std::ifstream f(ruta, std::ios::binary | std::ios::ate);
  if (!f) return 0;
  const long long bytes = static_cast<long long>(f.tellg());
  return static_cast<uint64_t>(bytes);
}

void imprimirUso() {
  std::cout << "Uso: ./streaming [ruta_csv] [ruta_estado] [--sin-cache]\n"
               "  ruta_csv     por defecto data/wiki_movie_plots_deduped.csv\n"
               "  ruta_estado  por defecto datos_usuario.txt\n"
               "  --sin-cache  fuerza reconstruir los arboles desde el CSV\n";
}

} // namespace

int main(int argc, char **argv) {
  std::ios::sync_with_stdio(false);

  std::string rutaCsv = "data/wiki_movie_plots_deduped.csv";
  std::string rutaEstado = kRutaEstadoPorDefecto;
  bool usarCache = true;
  std::vector<std::string> posicionales;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--sin-cache") {
      usarCache = false;
    } else if (arg == "-h" || arg == "--ayuda" || arg == "--help") {
      imprimirUso();
      return 0;
    } else {
      posicionales.push_back(arg);
    }
  }
  if (posicionales.size() > 0) rutaCsv = posicionales[0];
  if (posicionales.size() > 1) rutaEstado = posicionales[1];

  const std::string rutaCache = rutaCsv + ".cache";
  const uint64_t huella = huellaDeArchivo(rutaCsv);

  pf::Catalogo catalogo;
  pf::Indice indice;
  pf::EstadisticasIndice ei;
  bool desdeCache = false;

  if (usarCache && huella != 0) {
    if (indice.cargarCache(rutaCache, catalogo, huella, ei)) {
      desdeCache = true;
      std::cout << "Cache valido encontrado (" << rutaCache << ")\n"
                << "  peliculas             : " << catalogo.tamano() << "\n"
                << "  arranque              : " << std::fixed
                << std::setprecision(2) << ei.segundos << " s\n"
                << std::defaultfloat;
    }
  }

  if (!desdeCache) {
    std::cout << "Cargando catalogo desde " << rutaCsv << " ...\n";
    pf::EstadisticasCarga ec;
    try {
      ec = catalogo.cargarDesdeCSV(rutaCsv);
    } catch (const std::exception &e) {
      std::cerr << "ERROR: " << e.what() << "\n";
      imprimirUso();
      return 1;
    }
    std::cout << "  filas leidas          : " << ec.filasLeidas << "\n"
              << "  descartadas (titulo)  : " << ec.descartadasSinTitulo << "\n"
              << "  descartadas (sinopsis): " << ec.descartadasSinopsisCorta << "\n"
              << "  descartadas (duplicad): " << ec.descartadasDuplicadas << "\n"
              << "  director desconocido  : " << ec.directoresDesconocidos << "\n"
              << "  genero desconocido    : " << ec.generosDesconocidos << "\n"
              << "  reparto vacio         : " << ec.repartosVacios << "\n"
              << "  peliculas utiles      : " << catalogo.tamano() << "\n"
              << "  tiempo                : " << ec.segundos << " s\n\n";

    std::cout << "Construyendo arboles e indice ...\n";
    ei = indice.construir(catalogo);
    std::cout << "  vocabulario           : " << ei.terminos << " palabras\n"
              << "  publicaciones         : " << ei.publicaciones << "\n"
              << "  nodos trie prefijos   : " << ei.nodosPrefijos << "\n"
              << "  nodos trie sufijos    : " << ei.nodosSufijos << "\n"
              << "  memoria del indice    : ~" << ei.megabytes << " MB\n"
              << "  tiempo                : " << ei.segundos << " s\n";

    if (usarCache && huella != 0) {
      if (indice.guardarCache(rutaCache, catalogo, huella)) {
        std::cout << "  cache escrito en      : " << rutaCache
                  << " (el proximo arranque sera casi instantaneo)\n";
      }
    }
  }

  Aplicacion app(catalogo, indice, rutaEstado);
  app.ejecutar();
  return 0;
}
