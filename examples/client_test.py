#!/usr/bin/env python3
"""Client de test pour le framework servd.

Envoie une commande PING (0x01) ou LOGIN (0x02) et affiche la réponse.
S'il s'agit d'un LOGIN, le client reste à l'écoute pour recevoir les notifications Push (Broadcast).

Usage:
    python3 examples/client_test.py [--host HOST] [--port PORT] [--cmd COMMAND_ID] [--payload TEXT]
"""

import socket
import struct
import sys
import argparse

# Correspond au FrameHeader C++ (packed, 16 octets)
# uint16_t command_id, uint16_t flags, uint32_t payload_length, uint64_t session_id
FRAME_HEADER_FMT = "<HHIQ"  # little-endian: H=uint16, I=uint32, Q=uint64
HEADER_SIZE = struct.calcsize(FRAME_HEADER_FMT)  # 16

# Identifiants de commandes (doivent matcher le enum du serveur)
CMD_PING  = 0x01
CMD_LOGIN = 0x02


def build_frame(command_id: int, payload: bytes, session_id: int = 0) -> bytes:
    header = struct.pack(FRAME_HEADER_FMT, command_id, 0, len(payload), session_id)
    return header + payload


def parse_response(data: bytes) -> dict:
    if len(data) < HEADER_SIZE:
        return {"error": "Réponse trop courte"}
    command_id, flags, payload_len, session_id = struct.unpack(FRAME_HEADER_FMT, data[:HEADER_SIZE])

    return {
        "command_id": command_id,
        "flags": flags,
        "payload_length": payload_len,
        "session_id": session_id,
        "payload": b"",
        "payload_text": "",
    }


def recv_exact(sock: socket.socket, n: int) -> bytes:
    """Lit exactement n octets (gère le streaming TCP)."""
    chunks = []
    received = 0
    while received < n:
        chunk = sock.recv(n - received)
        if not chunk:
            raise ConnectionError("Connexion fermée par le serveur")
        chunks.append(chunk)
        received += len(chunk)
    return b"".join(chunks)


def main():
    parser = argparse.ArgumentParser(description="Test client pour servd")
    parser.add_argument("--host", default="127.0.0.1", help="Adresse du serveur")
    parser.add_argument("--port", type=int, default=8080, help="Port TCP")
    parser.add_argument("--cmd", type=lambda x: int(x, 0), default=CMD_PING,
                        help="ID de commande (hex ou décimal)")
    parser.add_argument("--payload", default="Hello from Python!", help="Payload texte")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)

    try:
        print(f"[*] Connexion à {args.host}:{args.port}...")
        sock.connect((args.host, args.port))
        print("[+] Connecté.")

        # 1. Envoi de la trame
        payload_bytes = args.payload.encode("utf-8")
        frame = build_frame(args.cmd, payload_bytes)
        print(f"[>] Envoi commande 0x{args.cmd:04X} | payload={len(payload_bytes)} octets")
        sock.sendall(frame)

        # 2. Réception de la réponse immédiate (header d'abord, puis payload)
        header_data = recv_exact(sock, HEADER_SIZE)
        resp = parse_response(header_data)

        if resp["payload_length"] > 0:
            payload_data = recv_exact(sock, resp["payload_length"])
            resp["payload"] = payload_data
            resp["payload_text"] = payload_data.decode("utf-8", errors="replace")

        print(f"[<] Réponse:")
        print(f"    command_id:  0x{resp['command_id']:04X}")
        print(f"    flags:       0x{resp['flags']:04X}")
        print(f"    session_id:  {resp['session_id']}")
        print(f"    payload:     {resp['payload_text']}")

        # 3. Écoute des tâches d'arrière-plan (Serveur -> Client)
        # Si on s'est loggé, on reste branché pour recevoir les alertes systèmes !
        if args.cmd == CMD_LOGIN:
            print("\n⏳ Attente des notifications du serveur (12 secondes max)...")
            sock.settimeout(12.0)
            while True:
                try:
                    head = recv_exact(sock, HEADER_SIZE)
                    push = parse_response(head)

                    if push["payload_length"] > 0:
                        p_data = recv_exact(sock, push["payload_length"])
                        push["payload"] = p_data
                        push["payload_text"] = p_data.decode("utf-8", errors="replace")

                    print(f"  [🔔 PUSH REÇU] CMD: 0x{push['command_id']:02X} | Msg: {push['payload_text']}")

                except socket.timeout:
                    print("[-] Fin de l'écoute (Timeout normal).")
                    break

    except ConnectionRefusedError:
        print("[-] Connexion refusée — le serveur est-il lancé ?")
    except ConnectionError as ce:
        print(f"[-] Erreur réseau : {ce}")
    except Exception as e:
        print(f"[-] Erreur inattendue : {e}")
    finally:
        sock.close()


if __name__ == "__main__":
    main()