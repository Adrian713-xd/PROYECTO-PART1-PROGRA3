// buscador.hpp — Parseo de consultas y algoritmo de importancia (ranking).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "catalogo.hpp"
#include "indice.hpp"

namespace pf {

struct Resultado {
  int doc = -1;
  double puntaje = 0.0;
  std::string explicacion; // por que salio: util para la exposicion
};

// Un termino de la consulta, ya expandido contra el vocabulario.
struct TerminoConsulta {
  std::string patron;
  uint8_t campos = C_CUALQUIERA;
  // (idTermino, factor) donde factor = peso lexico x idf efectivo.
  std::vector<std::pair<int, double>> expansion;
};

struct Consulta {
  std::string textoNormalizado;
  std::vector<TerminoConsulta> terminos;
  int anio = -1;
  bool valida = false;
  std::string mensaje;
};

class Buscador {
public:
  Buscador(const Catalogo &catalogo, const Indice &indice);

  // Interpreta la linea escrita por el usuario. Reconoce filtros por etiqueta:
  //   director:nolan   reparto:"tom hanks"   genero:horror   anio:1994
  //   origen:american  titulo:barco
  // y texto libre (palabra, frase o sub-palabra).
  Consulta interpretar(const std::string &linea) const;

  // Devuelve hasta 'limite' resultados ordenados por importancia.
  std::vector<Resultado> buscar(const Consulta &q, size_t limite) const;

private:
  const Catalogo &catalogo_;
  const Indice &indice_;

  // Acumuladores reutilizados entre consultas. El patron "marca de version"
  // evita reinicializar 35 000 celdas en cada busqueda: una celda se considera
  // vacia si su marca no coincide con la de la consulta/termino en curso.
  mutable std::vector<double> acumulado_;
  mutable std::vector<double> mejorAporte_;
  mutable std::vector<uint16_t> coincidencias_;
  mutable std::vector<uint32_t> marcaConsulta_;
  mutable std::vector<uint32_t> marcaTermino_;
  mutable uint32_t contadorConsulta_ = 0;
  mutable uint32_t contadorTermino_ = 0;

  std::vector<std::pair<int, double>> expandir(const std::string &patron) const;
  void reordenarPorFrase(const Consulta &q, std::vector<Resultado> &res) const;
  void explicar(const Consulta &q, std::vector<Resultado> &res) const;
};

} // namespace pf
