#include <cstring>
#include <servd/Server.hpp>
#include <iostream>
#include <string>

using namespace servd;

// Définition de nos identifiants de commandes (Le Protocole)
enum Commands : uint16_t {
    CMD_PING = 0x01,
    CMD_LOGIN = 0x02,
    CMD_SYS_ALERT = 0x99 // Poussé par le serveur
};

int main() {
    Server app;

    // 1. Configuration des écoutes
    app.enable_tcp(8080)
       .enable_udp(8081)
       .enable_unix_socket("/tmp/servd_test.sock");

    // 2. Une route PING publique (TCP ou UDP)
    app.add_command(CMD_PING, [](Context& ctx) -> Task<ResponseFrame> {
        std::string msg = "PONG ! Depuis le transport ID " + std::to_string((int)ctx.current_transport());

        std::vector<std::byte> response(msg.size());
        std::memcpy(response.data(), msg.data(), msg.size());

        co_return ResponseFrame{0, response};
    });

    // 3. Une route LOGIN sécurisée (Uniquement TCP)
    app.add_command(CMD_LOGIN, [](Context& ctx) -> Task<ResponseFrame> {
        ctx.session().set_authenticated(true, "SuperAdmin");
        std::cout << "[App] Client " << ctx.session().id() << " s'est connecté !" << std::endl;

        co_return ResponseFrame{0, {}}; // OK
    }).tcp_only();

    // 4. Une tâche en arrière-plan sans thread bloquant !
    app.add_periodic_task(std::chrono::seconds(10), [](Server& server) -> Task<void> {
        std::string alert = "Alerte système : Tout va bien !";
        std::vector<std::byte> payload(alert.size());
        std::memcpy(payload.data(), alert.data(), alert.size());

        // Envoi à tous les clients connectés et authentifiés
        co_await server.broadcast_if(CMD_SYS_ALERT, payload, [](const Session& s) {
            return s.is_authenticated();
        });
    });

    // 5. Lancement
    std::cout << "[App] Démarrage du Serveur ServD..." << std::endl;
    app.init();
    app.run();

    return 0;
}