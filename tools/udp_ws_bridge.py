#!/usr/bin/env python3
from __future__ import annotations

import argparse
import asyncio
import base64
import hashlib
import socket
import struct
import sys
from typing import Optional


class WebSocketClient:
    def __init__(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        self.reader = reader
        self.writer = writer
        self.alive = True

    async def close(self) -> None:
        if not self.alive:
            return
        self.alive = False
        try:
            self.writer.close()
            await self.writer.wait_closed()
        except Exception:
            pass

    async def send_text(self, message: str) -> None:
        if not self.alive:
            return
        payload = message.encode("utf-8")
        header = bytearray()
        header.append(0x81)
        length = len(payload)
        if length < 126:
            header.append(length)
        elif length < (1 << 16):
            header.append(126)
            header.extend(struct.pack("!H", length))
        else:
            header.append(127)
            header.extend(struct.pack("!Q", length))
        self.writer.write(header + payload)
        await self.writer.drain()


class Bridge:
    def __init__(self, multicast_address: str, port: int) -> None:
        self.multicast_address = multicast_address
        self.port = port
        self.clients: set[WebSocketClient] = set()
        self.lock = asyncio.Lock()
        self.packet_count = 0

    async def add_client(self, client: WebSocketClient) -> None:
        async with self.lock:
            self.clients.add(client)

    async def remove_client(self, client: WebSocketClient) -> None:
        async with self.lock:
            self.clients.discard(client)

    async def broadcast(self, message: str) -> None:
        async with self.lock:
            clients = list(self.clients)
        if not clients:
            return
        await asyncio.gather(*(client.send_text(message) for client in clients), return_exceptions=True)
        for client in clients:
            if not client.alive:
                await self.remove_client(client)


class MulticastProtocol(asyncio.DatagramProtocol):
    def __init__(self, bridge: Bridge) -> None:
        self.bridge = bridge

    def datagram_received(self, data: bytes, addr) -> None:
        try:
            message = data.decode("utf-8")
        except UnicodeDecodeError:
            return
        self.bridge.packet_count += 1
        if self.bridge.packet_count % 25 == 0:
            print(f"UDP packets received: {self.bridge.packet_count}", flush=True)
        asyncio.create_task(self.bridge.broadcast(message))


async def read_http_headers(reader: asyncio.StreamReader) -> dict[str, str]:
    request_line = await reader.readline()
    if not request_line:
        return {}
    headers: dict[str, str] = {}
    while True:
        line = await reader.readline()
        if line in (b"", b"\r\n"):
            break
        text = line.decode("latin-1").rstrip("\r\n")
        if ":" not in text:
            continue
        key, value = text.split(":", 1)
        headers[key.strip().lower()] = value.strip()
    return headers


async def handle_websocket(reader: asyncio.StreamReader, writer: asyncio.StreamWriter, bridge: Bridge) -> None:
    try:
        headers = await read_http_headers(reader)
        key = headers.get("sec-websocket-key")
        if not key:
            writer.close()
            await writer.wait_closed()
            return

        accept = base64.b64encode(
            hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")).digest()
        ).decode("ascii")
        writer.write(
            (
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                f"Sec-WebSocket-Accept: {accept}\r\n\r\n"
            ).encode("ascii")
        )
        await writer.drain()

        client = WebSocketClient(reader, writer)
        await bridge.add_client(client)
        try:
            while True:
                first = await reader.readexactly(2)
                fin = (first[0] & 0x80) != 0
                opcode = first[0] & 0x0F
                masked = (first[1] & 0x80) != 0
                length = first[1] & 0x7F
                if length == 126:
                    length = struct.unpack("!H", await reader.readexactly(2))[0]
                elif length == 127:
                    length = struct.unpack("!Q", await reader.readexactly(8))[0]
                mask = await reader.readexactly(4) if masked else b""
                payload = await reader.readexactly(length) if length else b""
                if masked and payload:
                    payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
                if opcode == 0x8:
                    break
                if opcode == 0x9:
                    writer.write(b"\x8A\x00")
                    await writer.drain()
                if not fin:
                    continue
        except (asyncio.IncompleteReadError, ConnectionError, OSError):
            pass
        finally:
            client.alive = False
            await bridge.remove_client(client)
            try:
                writer.close()
                await writer.wait_closed()
            except Exception:
                pass
    except Exception:
        try:
            writer.close()
            await writer.wait_closed()
        except Exception:
            pass


async def start_multicast_listener(bridge: Bridge) -> asyncio.DatagramTransport:
    loop = asyncio.get_running_loop()

    def resolve_interface_address() -> str:
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            probe.connect((bridge.multicast_address, bridge.port))
            return probe.getsockname()[0]
        except OSError:
            return "0.0.0.0"
        finally:
            probe.close()

    interface_address = resolve_interface_address()
    print(f"Joining multicast on interface {interface_address}", flush=True)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(("0.0.0.0", bridge.port))
    except OSError:
        sock.bind(("", bridge.port))
    mreq = socket.inet_aton(bridge.multicast_address) + socket.inet_aton(interface_address)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
    transport, _ = await loop.create_datagram_endpoint(lambda: MulticastProtocol(bridge), sock=sock)
    return transport


async def main() -> int:
    parser = argparse.ArgumentParser(description="RecPlay UDP to WebSocket bridge")
    parser.add_argument("--multicast-address", default="239.1.1.1")
    parser.add_argument("--port", type=int, default=5000)
    parser.add_argument("--ws-host", default="127.0.0.1")
    parser.add_argument("--ws-port", type=int, default=8765)
    args = parser.parse_args()

    bridge = Bridge(args.multicast_address, args.port)
    transport = await start_multicast_listener(bridge)
    server = await asyncio.start_server(
        lambda r, w: handle_websocket(r, w, bridge),
        host=args.ws_host,
        port=args.ws_port,
    )
    print(
        f"Bridge listening on ws://{args.ws_host}:{args.ws_port} and "
        f"forwarding UDP {args.multicast_address}:{args.port}",
        flush=True,
    )
    print("Open tools/trajectory_viewer.html after this starts.", flush=True)

    try:
        async with server:
            await server.serve_forever()
    finally:
        transport.close()
        server.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
