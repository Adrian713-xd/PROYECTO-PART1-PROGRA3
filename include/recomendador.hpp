// recomendador.hpp — Peliculas similares a las que el usuario dio "Me gusta".
#pragma once
#include <vector>

#include "buscador.hpp"
#include "catalogo.hpp"
#include "indice.hpp"

namespace pf {

class Recomendador {
public:
  Recomendador(const Catalogo &catalogo, const Indice &indice);

  // Similares a UNA pelicula (opcion "ver similares" en la ficha).
  std::vector<Resultado> similaresA(int doc, size_t limite,
                                    const std::vector<int> &excluir = {}) const;

  // Recomendaciones para el usuario: se construye un perfil sumando los
  // vectores tf-idf de todas las peliculas con "Me gusta" y se buscan los
  // vecinos mas cercanos por similitud del coseno.
  std::vector<Resultado> paraUsuario(const std::vector<int> &megusta,
                                     const std::vector<int> &excluir,
                                     size_t limite) const;

private:
  const Catalogo &catalogo_;
  const Indice &indice_;

  mutable std::vector<double> acumulador_;
  mutable std::vector<uint32_t> marca_;
  mutable uint32_t contador_ = 0;

  std::vector<Resultado> vecinos(const std::vector<TerminoPeso> &perfil,
                                 double normaPerfil,
                                 const std::vector<int> &excluir,
                                 size_t limite) const;
};

} // namespace pf
