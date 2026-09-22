// catalogo.hpp — Modelo de datos y pre-procesamiento del dataset.
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "binario.hpp"

namespace pf {

struct Pelicula {
  int anio = 0;
  std::string titulo;      // texto original, para mostrar
  std::string tituloNorm;  // normalizado, para ranking y deduplicacion
  std::string origen;      // Origin/Ethnicity
  std::string director;    // vacio si el dataset dice "Unknown"
  std::vector<std::string> reparto;
  std::vector<std::string> generos; // vacio si el dataset dice "unknown"
  std::string url;
  std::string sinopsis;    // limpia (sin "[1]" ni CRLF)
  unsigned long numPalabrasSinopsis = 0;
};

// Contadores del pre-procesamiento, para reportarlos al usuario y en el README.
struct EstadisticasCarga {
  size_t filasLeidas = 0;
  size_t descartadasSinTitulo = 0;
  size_t descartadasSinopsisCorta = 0;
  size_t descartadasDuplicadas = 0;
  size_t directoresDesconocidos = 0;
  size_t generosDesconocidos = 0;
  size_t repartosVacios = 0;
  double segundos = 0.0;
};

class Catalogo {
public:
  // Lee el CSV y aplica todas las reglas de limpieza. Lanza std::runtime_error
  // si el archivo no existe o le faltan columnas obligatorias.
  EstadisticasCarga cargarDesdeCSV(const std::string &ruta);

  size_t tamano() const { return peliculas_.size(); }
  const Pelicula &operator[](size_t i) const { return peliculas_[i]; }
  const std::vector<Pelicula> &peliculas() const { return peliculas_; }

  // Clave estable "titulonorm|anio", usada para persistir Me gusta / Ver mas
  // tarde sin depender del orden de las filas del CSV.
  std::string clave(size_t i) const;
  int buscarPorClave(const std::string &clave) const;

  // Serializacion para el cache binario (ver Indice::guardarCache).
  void escribir(EscritorBinario &e) const;
  bool leer(LectorBinario &l);

private:
  std::vector<Pelicula> peliculas_;
  // Indice perezoso clave -> posicion, para EstadoUsuario::cargar.
  mutable std::unordered_map<std::string, int> indiceClaves_;
};

} // namespace pf
