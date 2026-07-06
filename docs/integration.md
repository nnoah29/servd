# Intégrer servd dans votre projet

## Via CMake

### Après installation système

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

```cmake
find_package(servd REQUIRED)
target_link_libraries(my_app PRIVATE servd::servd_core)
```

### Via CPM.cmake

```cmake
include(cmake/CPM.cmake)
CPMAddPackage(NAME servd GITHUB_REPOSITORY user/servd VERSION 0.1.0)
target_link_libraries(my_app PRIVATE servd::servd_core)
```

### Via add_subdirectory

```cmake
add_subdirectory(path/to/servd)
target_link_libraries(my_app PRIVATE servd_core)
```

## Headers publics

```cpp
#include <servd/Server.hpp>        // Classe principale
#include <servd/Protocol.hpp>      // FrameHeader, constantes
#include <servd/Context.hpp>       // Contexte de requête
#include <servd/interfaces/*.hpp>  // Interfaces (IAuthenticator, ISessionStore, etc.)
#include <servd/auth/*.hpp>        // Implémentations d'auth
#include <servd/store/*.hpp>       // Implémentations de stores
#include <servd/crypto/*.hpp>      // Chiffrement
#include <Logger/Logger.hpp>       // Journalisation
```

## Structure de projet minimale

```
mon-app/
├── CMakeLists.txt
└── src/
    └── main.cpp
```

```cmake
cmake_minimum_required(VERSION 3.16)
project(mon_app CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(servd REQUIRED)
add_executable(mon_app src/main.cpp)
target_link_libraries(mon_app PRIVATE servd::servd_core)
```
