# FAQ et dépannage

## FAQ

### Qu'est-ce que servd ?

servd est un framework serveur asynchrone C++20 haute performance, mono-thread, basé sur `io_uring` et les coroutines. Il est conçu pour les applications M2M (Machine-to-Machine) nécessitant de faibles latences et un haut débit.

### Pourquoi `io_uring` plutôt qu'epoll ?

`io_uring` permet de soumettre des opérations d'E/S et de récupérer leurs résultats sans aucun syscall dans le chemin critique (grâce aux ring buffers en mémoire partagée). Cela offre :
- Zéro syscall par requête (vs. 4+ syscalls avec epoll + read/write)
- Zéro allocation sur le chemin critique
- Support natif des opérations asynchrones (accept, read, write, timeout, etc.)

### Pourquoi mono-thread ?

- Pas de mutex, pas de deadlock, pas de data race
- Pas de coût de synchronisation
- Modèle mental simple : tout s'exécute séquentiellement
- Les coroutines permettent la concurrence sans parallélisme

Pour les charges CPU-bound, il est possible de lancer plusieurs instances du serveur.

### Quelle est la différence entre mode binaire et texte ?

| Mode | Taille header | Performance | Usage |
|---|---|---|---|
| Binaire | 16 bytes | Optimale | Production |
| Texte | Variable | ~10× plus lent | Debug, interop |

Le mode texte lit octet par octet pour détecter les fins de ligne, d'où sa lenteur.

### Comment sont gérées les sessions ?

Les sessions sont stockées via un `ISessionStore` injectable. Par défaut, `InMemorySessionStore` est utilisé (en mémoire, perdu au redémarrage). Vous pouvez fournir votre propre implémentation (Redis, SQLite, fichier).

### Le chiffrement est-il obligatoire ?

Non. Le chiffrement de bout en bout (X25519 + AES-256-GCM) est optionnel. Sans lui, les données transitent en clair sur le réseau (mais le protocole binaire reste efficace).

## Problèmes courants

### `io_uring_queue_init` échoue

**Symptôme** : Le serveur crash au démarrage avec `io_uring_queue_init failed`.

**Cause** : Kernel Linux trop vieux (< 5.1) ou limite de mémoire verrouillée (`memlock`) insuffisante.

**Solution** :
```bash
# Vérifier la version du kernel
uname -r

# Augmenter la limite de mémoire verrouillée
ulimit -l unlimited

# Ou en configuration système
echo '* soft memlock unlimited' >> /etc/security/limits.conf
echo '* hard memlock unlimited' >> /etc/security/limits.conf
```

### `Permission denied` sur le port

**Symptôme** : `bind: Permission denied` lors de l'initialisation.

**Cause** : Les ports < 1024 nécessitent `root`, ou `SO_REUSEPORT` nécessite `CAP_NET_ADMIN`.

**Solution** :
```bash
# Utiliser un port > 1024
app.enable_tcp(8080);  // OK

# Ou exécuter en tant que root
sudo ./build/servd
```

### Le serveur ne répond pas

**Symptôme** : Le client se connecte mais ne reçoit aucune réponse.

**Causes possibles** :
1. La commande n'est pas enregistrée : `app.add_command(CMD_ID, handler)`
2. La commande nécessite une authentification mais le client n'est pas authentifié
3. Le handler ne `co_return` pas
4. Le handler lève une exception non catchée

**Solution** : Activez `Logger::setLevel(LogLevel::DEBUG)` et vérifiez les logs.

### Erreur de compilation : `coroutine` header not found

**Symptôme** : Erreur `fatal error: coroutine: No such file or directory`.

**Cause** : Le compilateur est trop vieux (GCC < 11, Clang < 14) ou le standard C++20 n'est pas activé.

**Solution** :
```bash
# Vérifier la version du compilateur
g++ --version

# Dans CMakeLists.txt
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### Botan non trouvé

**Symptôme** : `CMake Error: Could not find a package configuration file for "Botan"`.

**Solution** :
```bash
# Installer Botan
sudo apt install libbotan-3-dev

# Vérifier
find_package(Botan)
```

### Le serveur ne compile pas avec `-DCMAKE_BUILD_TYPE=Release`

**Symptôme** : Erreurs de compilation étranges.

**Solution** : Assurez-vous que les optimisations du compilateur ne cassent pas les packed structs. Ajoutez `-fno-strict-aliasing` si nécessaire.

### `SIGPIPE` tue le serveur

**Symptôme** : Le serveur se termine sans message d'erreur.

**Cause** : `SIGPIPE` est envoyé quand on écrit sur une socket fermée.

**Solution** : `SIGPIPE` est déjà ignoré dans `UringEngine::run()`. Si vous avez un handler qui écrit sur une socket fermée, vérifiez que la session est toujours active.

### Timeout sur `async_read_exact`

**Symptôme** : La lecture d'une trame ne se termine jamais.

**Cause** : Le client n'envoie pas assez de données (ex: payload_length annoncé > données envoyées).

**Solution** : Vérifiez que le client construit correctement le `FrameHeader` avec le bon `payload_length`.

## Débogage

### Activer les logs DEBUG

```cpp
Logger::setLevel(LogLevel::DEBUG);
```

### Vérifier les trames échangées

Utilisez Wireshark/TCP dump :
```bash
sudo tcpdump -i lo -X port 8080
```

### Tester avec netcat

```bash
# Connexion TCP (mode texte uniquement)
echo -e "1 0 0\nping" | nc localhost 8080

# Mode binaire (hex)
printf '\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00' | nc localhost 8080 | xxd
```

### Vérifier le nombre de connexions actives

```bash
ss -tlnp | grep 8080
lsof -i :8080
```
