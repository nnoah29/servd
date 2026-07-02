cmake_minimum_required(VERSION 4.2)
project(servd)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(servd_core
        src/Server.cpp
        src/UringEngine.cpp
        Logger/Logger.cpp
        include/servd/Context.hpp
        include/servd/Protocol.hpp
        include/servd/Server.hpp
        include/servd/router/Router.hpp
        include/servd/router/Endpoint.hpp
        include/servd/interfaces/ISessionStore.hpp
        include/servd/interfaces/IConnection.hpp
        include/servd/interfaces/Session.hpp
        include/servd/Task.hpp
)

target_include_directories(servd_core PUBLIC include Logger)

add_executable(servd app/main.cpp
        include/servd/Context.hpp
        include/servd/Protocol.hpp
        include/servd/Server.hpp
        include/servd/router/Router.hpp
        include/servd/router/Endpoint.hpp
        include/servd/interfaces/ISessionStore.hpp
        include/servd/interfaces/IConnection.hpp
        include/servd/interfaces/Session.hpp
        include/servd/Task.hpp)
target_link_libraries(servd_core PUBLIC uring)
target_link_libraries(servd PRIVATE servd_core)
