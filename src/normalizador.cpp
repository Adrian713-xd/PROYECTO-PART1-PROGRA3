#include "normalizador.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <unordered_set>

namespace pf {
namespace {

// Latin Extended-A (U+0100 .. U+017F) -> letra base ASCII.
const char *kLatinExtA =
    "aaaaaa"          // 0100-0105
    "cccccccc"        // 0106-010D
    "dddd"            // 010E-0111
    "eeeeeeeeee"      // 0112-011B
    "gggggggg"        // 011C-0123
    "hhhh"            // 0124-0127
    "iiiiiiiiiiii"    // 0128-0133
    "jj"              // 0134-0135
    "kkk"             // 0136-0138
    "llllllllll"      // 0139-0142
    "nnnnnnnnn"       // 0143-014B
    "oooooooo"        // 014C-0153
    "rrrrrr"          // 0154-0159
    "ssssssss"        // 015A-0161
    "tttttt"          // 0162-0167
    "uuuuuuuuuuuu"    // 0168-0173
    "ww"              // 0174-0175
    "yyy"             // 0176-0178
    "zzzzzz"          // 0179-017E
    "s";              // 017F

// Decodifica un punto de codigo UTF-8 empezando en s[i]; avanza i.
// Devuelve 0xFFFD si la secuencia es invalida.
uint32_t decodificarUtf8(const std::string &s, size_t &i) {
  unsigned char b0 = static_cast<unsigned char>(s[i]);
  size_t restantes = s.size() - i;
  auto continuacion = [&](size_t k) {
    return k < restantes && (static_cast<unsigned char>(s[i + k]) & 0xC0) == 0x80;
  };
  if (b0 < 0x80) {
    ++i;
    return b0;
  }
  if ((b0 & 0xE0) == 0xC0 && continuacion(1)) {
    uint32_t cp = ((b0 & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
    i += 2;
    return cp;
  }
  if ((b0 & 0xF0) == 0xE0 && continuacion(1) && continuacion(2)) {
    uint32_t cp = ((b0 & 0x0Fu) << 12) |
                  ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) |
                  (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
    i += 3;
    return cp;
  }
  if ((b0 & 0xF8) == 0xF0 && continuacion(1) && continuacion(2) && continuacion(3)) {
    uint32_t cp = ((b0 & 0x07u) << 18) |
                  ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) |
                  ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) |
                  (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
    i += 4;
    return cp;
  }
  ++i;
  return 0xFFFD;
}

// Pliega un punto de codigo a su equivalente ASCII (cadena vacia si no aplica).
const char *plegar(uint32_t cp) {
  switch (cp) {
  case 0xC6: case 0xE6: return "ae";
  case 0xDF: return "ss";
  case 0xD8: case 0xF8: return "o";
  case 0xD0: case 0xF0: return "d";
  case 0xDE: case 0xFE: return "th";
  default: break;
  }
  if (cp >= 0xC0 && cp <= 0xC5) return "a";
  if (cp == 0xC7) return "c";
  if (cp >= 0xC8 && cp <= 0xCB) return "e";
  if (cp >= 0xCC && cp <= 0xCF) return "i";
  if (cp == 0xD1) return "n";
  if (cp >= 0xD2 && cp <= 0xD6) return "o";
  if (cp >= 0xD9 && cp <= 0xDC) return "u";
  if (cp == 0xDD) return "y";
  if (cp >= 0xE0 && cp <= 0xE5) return "a";
  if (cp == 0xE7) return "c";
  if (cp >= 0xE8 && cp <= 0xEB) return "e";
  if (cp >= 0xEC && cp <= 0xEF) return "i";
  if (cp == 0xF1) return "n";
  if (cp >= 0xF2 && cp <= 0xF6) return "o";
  if (cp >= 0xF9 && cp <= 0xFC) return "u";
  if (cp == 0xFD || cp == 0xFF) return "y";
  if (cp >= 0x0100 && cp <= 0x017F) {
    static thread_local char buf[2] = {0, 0};
    buf[0] = kLatinExtA[cp - 0x0100];
    return buf;
  }
  return "";
}

const std::unordered_set<std::string> &stopwords() {
  static const std::unordered_set<std::string> s = {
      "a", "about", "above", "after", "again", "against", "all", "am", "an",
      "and", "any", "are", "as", "at", "be", "because", "been", "before",
      "being", "below", "between", "both", "but", "by", "can", "cannot",
      "could", "did", "do", "does", "doing", "down", "during", "each", "few",
      "for", "from", "further", "had", "has", "have", "having", "he", "her",
      "here", "hers", "herself", "him", "himself", "his", "how", "i", "if",
      "in", "into", "is", "it", "its", "itself", "just", "me", "more", "most",
      "my", "myself", "no", "nor", "not", "of", "off", "on", "once", "only",
      "or", "other", "ought", "our", "ours", "ourselves", "out", "over", "own",
      "s", "same", "she", "should", "so", "some", "such", "t", "than", "that",
      "the", "their", "theirs", "them", "themselves", "then", "there", "these",
      "they", "this", "those", "through", "to", "too", "under", "until", "up",
      "very", "was", "we", "were", "what", "when", "where", "which", "while",
      "who", "whom", "why", "will", "with", "would", "you", "your", "yours",
      "yourself", "yourselves"};
  return s;
}

} // namespace

std::string limpiarTextoBruto(const std::string &s) {
  std::string salida;
  salida.reserve(s.size());
  bool espacioPendiente = false;
  for (size_t i = 0; i < s.size();) {
    char c = s[i];
    // Referencias del tipo "[12]" o "[cita requerida]".
    if (c == '[') {
      size_t j = i + 1;
      while (j < s.size() && s[j] != ']' && s[j] != '\n' && j - i < 24) ++j;
      if (j < s.size() && s[j] == ']') {
        bool esReferencia = true;
        for (size_t k = i + 1; k < j; ++k) {
          unsigned char d = static_cast<unsigned char>(s[k]);
          if (!std::isdigit(d) && !std::isalpha(d) && d != ' ') esReferencia = false;
        }
        if (esReferencia && j > i + 1) {
          espacioPendiente = true;
          i = j + 1;
          continue;
        }
      }
    }
    if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
      espacioPendiente = !salida.empty();
      ++i;
      continue;
    }
    if (espacioPendiente) {
      salida.push_back(' ');
      espacioPendiente = false;
    }
    salida.push_back(c);
    ++i;
  }
  return salida;
}

std::string normalizar(const std::string &s) {
  std::string salida;
  salida.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    unsigned char b = static_cast<unsigned char>(s[i]);
    if (b < 0x80) {
      if (std::isalnum(b)) {
        salida.push_back(static_cast<char>(std::tolower(b)));
      } else if (b == '\'') {
        // "don't" -> "dont": el apostrofe no separa palabras.
      } else {
        salida.push_back(' ');
      }
      ++i;
      continue;
    }
    uint32_t cp = decodificarUtf8(s, i);
    const char *base = plegar(cp);
    if (*base) {
      salida.append(base);
    } else {
      salida.push_back(' ');
    }
  }
  return salida;
}

void tokenizarNormalizado(const std::string &normalizado,
                          std::vector<std::string> &salida) {
  size_t i = 0;
  const size_t n = normalizado.size();
  while (i < n) {
    while (i < n && normalizado[i] == ' ') ++i;
    size_t inicio = i;
    while (i < n && normalizado[i] != ' ') ++i;
    if (i > inicio) salida.emplace_back(normalizado, inicio, i - inicio);
  }
}

std::vector<std::string> tokenizar(const std::string &texto) {
  std::vector<std::string> salida;
  tokenizarNormalizado(normalizar(texto), salida);
  return salida;
}

bool esStopword(const std::string &token) {
  return stopwords().count(token) > 0;
}

std::string recortar(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && static_cast<unsigned char>(s[a]) <= ' ') ++a;
  while (b > a && static_cast<unsigned char>(s[b - 1]) <= ' ') --b;
  return s.substr(a, b - a);
}

std::vector<std::string> separar(const std::string &s, const char *separadores) {
  std::vector<std::string> partes;
  std::string actual;
  for (char c : s) {
    if (std::strchr(separadores, c) != nullptr && c != '\0') {
      std::string t = recortar(actual);
      if (!t.empty()) partes.push_back(t);
      actual.clear();
    } else {
      actual.push_back(c);
    }
  }
  std::string t = recortar(actual);
  if (!t.empty()) partes.push_back(t);
  return partes;
}

} // namespace pf
