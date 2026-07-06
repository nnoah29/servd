# Journalisation (Logger)

Le système de logging est un module autonome avec sortie console (ANSI colors) et fichier.

## Header

```cpp
#include <Logger/Logger.hpp>
```

N.B. : Le Logger est **en dehors** du namespace `servd`. C'est un module indépendant.

## Niveaux de log

```cpp
enum class LogLevel {
    DEBUG,  // Messages de débogage (cyan)
    INFO,   // Informations générales (vert)
    WARN,   // Avertissements (jaune)
    ERROR   // Erreurs (rouge)
};
```

## Macros

```cpp
LOG(INFO, "Serveur démarré sur le port %d", port);
LOGS(INFO, "Session créée: " << session_id);

LOG(ERROR, "Échec: %s", strerror(errno));
LOGS(ERROR, "Erreur fatale: " << message);
```

- `LOG(level, format, ...)` : Style printf avec format string
- `LOGS(level, format, ...)` : Style stream avec `operator<<`
- Les deux macros capturent automatiquement `__FILE__` et `__LINE__`

## Format de sortie

```
[2026-07-07 14:30:15.123] [INFO]  [ServerLifecycle.cpp:42] Serveur démarré sur le port 8080
```

### Couleurs ANSI

| Niveau | Couleur |
|---|---|
| DEBUG | Cyan |
| INFO | Vert |
| WARN | Jaune |
| ERROR | Rouge |

Les couleurs sont activées uniquement si la sortie stderr est un terminal (TTY). Les logs redirigés vers un fichier n'ont pas de couleurs.

## Configuration

```cpp
Logger::setLevel(LogLevel::DEBUG);      // Niveau minimum (par défaut: DEBUG)
Logger::setLogFile("/var/log/servd.log"); // Activer la sortie fichier
```

### Depuis le fichier de configuration

```ini
log_level=INFO
log_file=/var/log/servd.log
```

Le chemin peut contenir `~` qui sera expansé vers le home directory. Les dossiers parents sont créés automatiquement.

## Implémentation

- Buffer de 2048 bytes pour le formatage
- Thread-safe via `std::mutex` (utilisé uniquement pour le logging, pas sur le chemin critique du serveur)
- Sortie vers `stderr` (pas `stdout` — évite les interférences avec les pipes UNIX)
- `print_sanitized` : les retours à la ligne dans les messages sont échappés pour la lisibilité

## Bonnes pratiques

```cpp
LOG(DEBUG, "Payload reçu: %zu bytes", payload.size());  // Verbose, désactivable

LOG(INFO, "Client connecté: %s", addr.c_str());

if (erreur) {
    LOG(WARN, "Tentative de connexion échouée: %s", reason);
}

LOG(ERROR, "Erreur irrécupérable: %s", strerror(errno));
```
