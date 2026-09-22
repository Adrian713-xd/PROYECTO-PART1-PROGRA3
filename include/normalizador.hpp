// normalizador.hpp — Limpieza y normalizacion de texto (Etapa de pre-procesamiento)
#pragma once
#include <string>
#include <vector>

namespace pf {

// Elimina referencias de Wikipedia ("[1]", "[cita requerida]"), retornos de
// carro incrustados y colapsa espacios repetidos. Conserva mayusculas y tildes
// porque este texto es el que se le muestra al usuario.
std::string limpiarTextoBruto(const std::string &s);

// Convierte a la forma canonica usada por el indice: minusculas, sin tildes ni
// diacriticos (plegado UTF-8 -> ASCII) y con todo caracter no alfanumerico
// reemplazado por un espacio. El alfabeto resultante es [a-z0-9 ].
std::string normalizar(const std::string &s);

// Parte una cadena YA normalizada en tokens de [a-z0-9]+.
void tokenizarNormalizado(const std::string &normalizado,
                          std::vector<std::string> &salida);

// Atajo: normalizar() + tokenizarNormalizado().
std::vector<std::string> tokenizar(const std::string &texto);

// Palabras vacias del ingles (el corpus de sinopsis esta en ingles).
bool esStopword(const std::string &token);

// Recorta espacios al inicio y al final.
std::string recortar(const std::string &s);

// Separa por cualquiera de los caracteres indicados, recortando cada parte y
// descartando las vacias.
std::vector<std::string> separar(const std::string &s, const char *separadores);

} // namespace pf
