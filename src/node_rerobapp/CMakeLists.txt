cmake_minimum_required(VERSION 3.10)

set(LIB_PATH /home/yecid/Desktop/docker/rerobapp_gui/libs)

# Nombre del proyecto
project(NetworkApp)

# Establecer el estándar C++ requerido
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Encontrar la biblioteca pthread para enlazar con el objetivo
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Incluir directorios para la biblioteca JSON
include_directories("${LIB_PATH}/json/include")
include_directories("${LIB_PATH}/rerobapp_libs")

set(SOURCES
    ${LIB_PATH}/rerobapp_libs/utils_network.cpp
)

# Definir los ejecutables
add_executable(Listener udp_listener.cpp ${SOURCES})
add_executable(Sender udp_sender.cpp ${SOURCES})

# Enlazar con pthread y la biblioteca de hilos de C++ para cada ejecutable
target_link_libraries(Listener PRIVATE Threads::Threads)
target_link_libraries(Sender PRIVATE Threads::Threads)

# Definir opciones de compilación para cada ejecutable
target_compile_options(Listener PRIVATE -Wall -Wextra -pthread)
target_compile_options(Sender PRIVATE -Wall -Wextra -pthread)
