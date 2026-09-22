// estado_usuario.hpp — "Me gusta" y "Ver mas tarde" con persistencia en disco.
#pragma once
#include <string>
#include <vector>

#include "catalogo.hpp"

namespace pf {

class EstadoUsuario {
public:
  // Se guarda por clave "titulo|anio" y no por indice de fila: asi el estado
  // sobrevive a cambios en el CSV o en el orden de carga.
  void cargar(const std::string &ruta, const Catalogo &catalogo);
  bool guardar(const std::string &ruta, const Catalogo &catalogo) const;

  bool alternarMeGusta(int doc);     // devuelve el estado nuevo
  bool alternarVerMasTarde(int doc); // devuelve el estado nuevo
  void marcarVisto(int doc);

  bool tieneMeGusta(int doc) const;
  bool tieneVerMasTarde(int doc) const;

  const std::vector<int> &meGusta() const { return meGusta_; }
  const std::vector<int> &verMasTarde() const { return verMasTarde_; }
  const std::vector<int> &vistas() const { return vistas_; }

private:
  std::vector<int> meGusta_;     // en orden de agregado
  std::vector<int> verMasTarde_;
  std::vector<int> vistas_;
};

} // namespace pf
