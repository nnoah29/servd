# Guide d'installation

## Méthode 1 : Compilation depuis les sources

### Prérequis

```bash
# Debian/Ubuntu
sudo apt install cmake g++ libbotan-3-dev liburing-dev

# Arch Linux
sudo pacman -S cmake gcc botan liburing

# Fedora
sudo dnf install cmake gcc-c++ botan-devel liburing-devel
```

### Compilation

```bash
git clone <url-du-depot> servd
cd servd
cmake -B build
cmake --build build
```

Les artefacts produits :
- `build/servd` : Exécutable de démonstration
- `build/libservd_core.a` : Bibliothèque statique

### Installation système

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

Ceci installe :
- `/usr/local/lib/libservd_core.a`
- `/usr/local/include/servd/*.hpp`
- `/usr/local/lib/cmake/servd/servdConfig.cmake`

## Méthode 2 : Intégration dans un projet CMake

### Via `find_package` (après installation)

```cmake
find_package(servd REQUIRED)
target_link_libraries(my_app PRIVATE servd::servd_core)
```

### Via `CPM.cmake`

```cmake
include(cmake/CPM.cmake)
CPMAddPackage(
    NAME servd
    GITHUB_REPOSITORY user/servd
    VERSION 0.1.0
)
target_link_libraries(my_app PRIVATE servd::servd_core)
```

### Via `add_subdirectory`

```cmake
add_subdirectory(path/to/servd)
target_link_libraries(my_app PRIVATE servd_core)
```

## Vérification

```bash
./build/servd &
# Dans un autre terminal
python3 examples/client_test.py
```

Le client doit se connecter, envoyer PING, recevoir une réponse, puis envoyer LOGIN.
