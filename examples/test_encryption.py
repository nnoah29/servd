#!/usr/bin/env python3
"""Test X25519 + AES-256-GCM handshake with servd.

Prerequisites:
    pip install cryptography

Usage:
    # Terminal 1: start the server
    ./build/servd

    # Terminal 2: run the test
    python3 examples/test_encryption.py
"""

import socket
import struct
import os
import sys

from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey, X25519PublicKey
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

FRAME_FMT = "<HHIQ"
CMD_KEY_EXCHANGE     = 0x00F0
CMD_ENCRYPTED_MESSAGE = 0x00F1
CMD_PING = 0x01

def recv_exact(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("Connection closed")
        data += chunk
    return data

def send_frame(sock, cmd, payload, session_id=0):
    header = struct.pack(FRAME_FMT, cmd, 0, len(payload), session_id)
    sock.sendall(header + payload)

def recv_frame(sock):
    header = recv_exact(sock, 16)
    cmd, flags, plen, sid = struct.unpack(FRAME_FMT, header)
    payload = recv_exact(sock, plen) if plen > 0 else b""
    return cmd, flags, sid, payload

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)
    sock.connect(("127.0.0.1", 8080))
    print("[+] Connected to server")

    # ── X25519 Handshake ──────────────────────────────────
    print("[*] X25519 handshake...")
    client_sk = X25519PrivateKey.generate()
    client_pk = client_sk.public_key().public_bytes_raw()

    send_frame(sock, CMD_KEY_EXCHANGE, client_pk, session_id=42)
    print("[>] CMD_KEY_EXCHANGE sent")

    cmd, flags, sid, server_pk = recv_frame(sock)
    assert cmd == CMD_KEY_EXCHANGE, f"Unexpected response: {cmd:#x}"
    print(f"[<] Server public key received ({len(server_pk)} bytes)")

    shared = client_sk.exchange(X25519PublicKey.from_public_bytes(server_pk))
    print(f"[+] Shared secret: {shared.hex()[:16]}...")
    print()

    # ── Encrypted command ────────────────────────────────
    print("[*] Sending encrypted CMD_PING...")
    inner = struct.pack("<H", CMD_PING) + b"Hello crypte!"
    iv = os.urandom(12)
    aes = AESGCM(shared)
    ct = aes.encrypt(iv, inner, None)  # [ciphertext || tag]
    ct_body = ct[:-16]  # ciphertext sans le tag
    tag = ct[-16:]     # GCM tag (16 bytes)

    # Wire format spec: [12B IV][16B Tag][Ciphertext]
    outer_payload = iv + tag + ct_body
    send_frame(sock, CMD_ENCRYPTED_MESSAGE, outer_payload, session_id=42)
    print(f"[>] CMD_ENCRYPTED_MESSAGE sent ({len(outer_payload)} bytes)")

    # ── Encrypted response ───────────────────────────────
    cmd, flags, sid, resp = recv_frame(sock)
    assert cmd == CMD_ENCRYPTED_MESSAGE, f"Response not encrypted: {cmd:#x}"

    # Wire format: [12B IV][16B Tag][Ciphertext] → reassembler pour Botan: [ciphertext || tag]
    resp_iv = resp[:12]
    resp_tag = resp[12:28]
    resp_ct = resp[28:]
    resp_reassembled = resp_ct + resp_tag
    plain = aes.decrypt(resp_iv, resp_reassembled, None)
    inner_cmd = struct.unpack("<H", plain[:2])[0]
    inner_payload = plain[2:].decode()

    print(f"[<] Decrypted response:")
    print(f"    inner_command_id: 0x{inner_cmd:04X}")
    print(f"    inner_payload:    {inner_payload}")
    print()

    # ── Verification ─────────────────────────────────────
    if inner_cmd == CMD_PING:
        print("✅ TEST PASSED — X25519 handshake + AES-256-GCM functional")
    else:
        print(f"❌ Error: unexpected command {inner_cmd:#x}")
        sys.exit(1)

    sock.close()

if __name__ == "__main__":
    main()
