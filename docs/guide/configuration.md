# Configuration

## Transports

Le serveur peut exposer plusieurs transports simultanément :

```cpp
Server app;
app.enable_tcp(8080, ProtocolMode::BINARY);          // TCP binaire (recommandé)
app.enable_tcp(8081, ProtocolMode::TEXT);             // TCP texte (debug, 10× plus lent)
app.enable_udp(8082);                                  // UDP (toujours binaire)
app.enable_unix_socket("/tmp/servd.sock", ProtocolMode::BINARY);
```

Le mode texte est utile pour le débogage avec netcat, mais déconseillé en production.

## Limite de clients

```cpp
app.set_max_clients(100);
```

Les connexions au-delà sont immédiatement fermées.

## Fichier de configuration

```cpp
app.load_config("config/servd.conf");
```

Format (`.env`-style) :

```ini
tcp=8080
udp=8081
unix=/tmp/servd.sock
max_clients=100
log_level=INFO
log_file=/var/log/servd.log
```

## Journalisation

```cpp
#include <Logger/Logger.hpp>

Logger::setLevel(LogLevel::DEBUG);   // DEBUG, INFO, WARN, ERROR
Logger::setLogFile("/var/log/servd.log");

LOG(INFO, "Serveur démarré sur le port %d", port);
LOGS(ERROR, "Erreur: " << message);  // alternative stream
```

Les logs sont colorés sur la sortie TTY, sans couleur dans le fichier.
