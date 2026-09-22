cmake_minimum_required(VERSION 3.16)
project(streaming_utec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra)
endif()

set(FUENTES_LIB
  src/normalizador.cpp
  src/csv.cpp
  src/catalogo.cpp
  src/trie.cpp
  src/indice.cpp
  src/buscador.cpp
  src/recomendador.cpp
  src/estado_usuario.cpp
)

add_library(nucleo STATIC ${FUENTES_LIB})
target_include_directories(nucleo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(streaming src/main.cpp)
target_link_libraries(streaming PRIVATE nucleo)

enable_testing()
add_executable(pruebas tests/pruebas.cpp)
target_link_libraries(pruebas PRIVATE nucleo)
add_test(NAME pruebas
         COMMAND pruebas
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
