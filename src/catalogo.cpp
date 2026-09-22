#include "catalogo.hpp"

#include "csv.hpp"
#include "normalizador.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace pf {
namespace {

// Minimo de caracteres para considerar que una sinopsis es util. Por debajo de
// esto el registro suele ser "No plot available." o similar.
constexpr size_t kMinCaracteresSinopsis = 40;

bool esDesconocido(const std::string &s) {
  std::string n = normalizar(s);
  n = recortar(n);
  return n.empty() || n == "unknown" || n == "n a" || n == "na" ||
         n == "not known" || n == "nan";
}

// El campo Cast usa comas; algunos registros usan tambien " and " o barras.
std::vector<std::string> separarReparto(const std::string &s) {
  std::string tmp = s;
  // Homogeneizar los separadores alternativos a comas.
  const char *patrones[] = {" and ", " / ", "/", ";", "|"};
  for (const char *p : patrones) {
    size_t pos = 0;
    const size_t len = std::strlen(p);
    while ((pos = tmp.find(p, pos)) != std::string::npos) {
      tmp.replace(pos, len, ",");
      pos += 1;
    }
  }
  std::vector<std::string> partes = separar(tmp, ",");
  std::vector<std::string> salida;
  for (auto &p : partes) {
    if (!esDesconocido(p) && p.size() > 1) salida.push_back(p);
  }
  if (salida.size() > 30) salida.resize(30); // registros con listas absurdas
  return salida;
}

std::vector<std::string> separarGeneros(const std::string &s) {
  std::vector<std::string> partes = separar(s, ",/|;");
  std::vector<std::string> salida;
  for (auto &p : partes) {
    if (!esDesconocido(p)) salida.push_back(p);
  }
  return salida;
}

} // namespace

EstadisticasCarga Catalogo::cargarDesdeCSV(const std::string &ruta) {
  using Reloj = std::chrono::steady_clock;
  const auto t0 = Reloj::now();

  EstadisticasCarga est;
  LectorCSV lector(ruta);

  const int cAnio = lector.columna("Release Year");
  const int cTitulo = lector.columna("Title");
  const int cOrigen = lector.columna("Origin/Ethnicity");
  const int cDirector = lector.columna("Director");
  const int cReparto = lector.columna("Cast");
  const int cGenero = lector.columna("Genre");
  const int cUrl = lector.columna("Wiki Page");
  const int cSinopsis = lector.columna("Plot");
  if (cTitulo < 0 || cSinopsis < 0) {
    throw std::runtime_error(
        "El CSV no tiene las columnas esperadas (Title / Plot).");
  }

  std::unordered_set<std::string> vistas;
  vistas.reserve(40000);
  peliculas_.reserve(35000);

  std::vector<std::string> fila;
  auto campo = [&](int idx) -> std::string {
    if (idx < 0 || idx >= static_cast<int>(fila.size())) return std::string();
    return recortar(fila[idx]);
  };

  while (lector.siguienteFila(fila)) {
    if (fila.size() < 2) continue;
    ++est.filasLeidas;

    Pelicula p;
    p.titulo = limpiarTextoBruto(campo(cTitulo));
    p.tituloNorm = recortar(normalizar(p.titulo));
    if (p.tituloNorm.empty()) {
      ++est.descartadasSinTitulo;
      continue;
    }

    p.sinopsis = limpiarTextoBruto(campo(cSinopsis));
    if (p.sinopsis.size() < kMinCaracteresSinopsis) {
      ++est.descartadasSinopsisCorta;
      continue;
    }

    const std::string textoAnio = campo(cAnio);
    p.anio = textoAnio.empty() ? 0 : std::atoi(textoAnio.c_str());
    if (p.anio < 1870 || p.anio > 2100) p.anio = 0;

    // Deduplicacion: mismo titulo normalizado + anio + director.
    const std::string directorBruto = campo(cDirector);
    const std::string claveDup =
        p.tituloNorm + "|" + std::to_string(p.anio) + "|" + normalizar(directorBruto);
    if (!vistas.insert(claveDup).second) {
      ++est.descartadasDuplicadas;
      continue;
    }

    if (esDesconocido(directorBruto)) {
      ++est.directoresDesconocidos;
    } else {
      p.director = limpiarTextoBruto(directorBruto);
    }

    const std::string generoBruto = campo(cGenero);
    p.generos = separarGeneros(generoBruto);
    if (p.generos.empty()) ++est.generosDesconocidos;

    p.reparto = separarReparto(campo(cReparto));
    if (p.reparto.empty()) ++est.repartosVacios;

    p.origen = campo(cOrigen);
    if (esDesconocido(p.origen)) p.origen.clear();
    p.url = campo(cUrl);

    peliculas_.push_back(std::move(p));
  }

  peliculas_.shrink_to_fit();
  est.segundos = std::chrono::duration<double>(Reloj::now() - t0).count();
  return est;
}

void Catalogo::escribir(EscritorBinario &e) const {
  e.pod<uint64_t>(peliculas_.size());
  for (const Pelicula &p : peliculas_) {
    e.pod<int32_t>(p.anio);
    e.cadena(p.titulo);
    e.cadena(p.tituloNorm);
    e.cadena(p.origen);
    e.cadena(p.director);
    e.vectorCadenas(p.reparto);
    e.vectorCadenas(p.generos);
    e.cadena(p.url);
    e.cadena(p.sinopsis);
  }
}

bool Catalogo::leer(LectorBinario &l) {
  uint64_t n = 0;
  if (!l.pod(n)) return false;
  peliculas_.clear();
  peliculas_.resize(static_cast<size_t>(n));
  for (uint64_t i = 0; i < n; ++i) {
    Pelicula &p = peliculas_[static_cast<size_t>(i)];
    int32_t anio = 0;
    if (!l.pod(anio)) return false;
    p.anio = anio;
    if (!l.cadena(p.titulo) || !l.cadena(p.tituloNorm) || !l.cadena(p.origen) ||
        !l.cadena(p.director) || !l.vectorCadenas(p.reparto) ||
        !l.vectorCadenas(p.generos) || !l.cadena(p.url) ||
        !l.cadena(p.sinopsis)) {
      return false;
    }
  }
  return true;
}

std::string Catalogo::clave(size_t i) const {
  return peliculas_[i].tituloNorm + "|" + std::to_string(peliculas_[i].anio);
}

int Catalogo::buscarPorClave(const std::string &c) const {
  if (indiceClaves_.size() != peliculas_.size()) {
    indiceClaves_.clear();
    indiceClaves_.reserve(peliculas_.size() * 2);
    for (size_t i = 0; i < peliculas_.size(); ++i) {
      indiceClaves_.emplace(clave(i), static_cast<int>(i));
    }
  }
  auto it = indiceClaves_.find(c);
  return it == indiceClaves_.end() ? -1 : it->second;
}

} // namespace pf
