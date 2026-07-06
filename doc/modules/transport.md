# Transports

servd supporte trois transports réseau : TCP, UDP, et Unix sockets.

## Configuration

Tous les transports sont configurés via l'interface fluide de `Server` :

```cpp
Server app;
app.enable_tcp(8080, ProtocolMode::BINARY);             // TCP binaire
app.enable_tcp(8081, ProtocolMode::TEXT);                // TCP texte (debug)
app.enable_udp(8082);                                    // UDP binaire
app.enable_unix_socket("/tmp/servd.sock", ProtocolMode::BINARY);
```

### TCP

- Utilise `SOCK_STREAM` avec `SO_REUSEADDR` et `SO_REUSEPORT`
- Chaque connexion client est gérée par une coroutine dédiée (`handle_client` ou `text_handle_client`)
- Supporte les modes **binaire** et **texte**

### UDP

- Utilise `SOCK_DGRAM`
- Buffer de réception de 64 KB
- Tous les datagrammes sont traités par une boucle unique (`start_udp_loop`)
- Mode binaire uniquement

### Unix socket

- Même mécanisme que TCP (`SOCK_STREAM`) sur socket Unix
- Le fichier de socket est `unlink()`é avant `bind()`
- Supporte les modes **binaire** et **texte**

## L'interface `IConnection`

Voir [IConnection](../api/interfaces.md#iconnection).

## Implémentations internes

### `UringTcpConnection` (connexion TCP binaire)

```cpp
class UringTcpConnection : public IConnection {
    int fd_;
    UringEngine* engine_;
public:
    TransportType transport_type() override { return TransportType::TCP; }
    Task<void> send_frame(const FrameHeader& header, bytes payload) override;
    std::string get_remote_address() override;
};
```

- `send_frame` : Sérialise le header + payload en un buffer contigu et appelle `async_write`
- Utilise une seule écriture pour tout le frame (pas de scatter/gather)

### `TextTcpConnection` (connexion TCP texte)

```cpp
class TextTcpConnection : public IConnection {
    int fd_;
    UringEngine* engine_;
public:
    TransportType transport_type() override { return TransportType::TCP; }
    Task<void> send_frame(const FrameHeader& header, bytes payload) override;
    std::string get_remote_address() override;
};
```

- `send_frame` : Formate en texte : `<cmd> <flags> <sid>\n<payload>\n`

### `UringUdpConnection` (connexion UDP)

```cpp
class UringUdpConnection : public IConnection {
    int fd_;
    UringEngine* engine_;
    sockaddr_in client_addr_;
public:
    TransportType transport_type() override { return TransportType::UDP; }
    Task<void> send_frame(const FrameHeader& header, bytes payload) override;
    std::string get_remote_address() override;
};
```

- `send_frame` : Sérialise header + payload et appelle `async_sendto` vers `client_addr_`
- `get_remote_address` : Formate `IP:port`

## Limite de clients

```cpp
app.set_max_clients(100);
```

Si le nombre de connexions actives dépasse `max_clients_`, les nouvelles connexions sont immédiatement fermées.

## Types de transport dans les contraintes de route

```cpp
// Commande accessible uniquement depuis TCP
app.add_command(CMD_PING, handler).tcp_only();

// Commande accessible uniquement depuis UDP
app.add_command(CMD_PING, handler).udp_only();

// Accessible depuis tous les transports (défaut)
app.add_command(CMD_PING, handler);
// Équivalent à : app.add_command(CMD_PING, handler).allowed_transport = ANY;
```

## Détection du transport dans un handler

```cpp
app.add_command(CMD_GET_INFO, [](Context& ctx) -> Task<ResponseFrame> {
    TransportType t = ctx.current_transport();
    std::string msg;
    switch (t) {
        case TransportType::TCP:  msg = "TCP"; break;
        case TransportType::UDP:  msg = "UDP"; break;
        case TransportType::UNIX: msg = "Unix"; break;
        default: msg = "Unknown";
    }
    auto payload = std::vector<std::byte>(msg.begin(), msg.end());
    return { { 0, payload } };
});
```
