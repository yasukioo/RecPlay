#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import socket
import time


def meters_per_degree_lat() -> float:
    return 111_320.0


def meters_per_degree_lon(lat_deg: float) -> float:
    return 111_320.0 * math.cos(math.radians(lat_deg))


def wrap_heading(deg: float) -> float:
    return deg % 360.0


def generate_packet(
    aircraft_id: str,
    seq: int,
    elapsed_ms: int,
    lat: float,
    lon: float,
    alt_m: float,
    hdg_deg: float,
    spd_kt: float,
) -> bytes:
    payload = {
        "id": aircraft_id,
        "seq": seq,
        "t_ms": elapsed_ms,
        "lat": round(lat, 6),
        "lon": round(lon, 6),
        "alt_m": round(alt_m, 1),
        "hdg_deg": round(wrap_heading(hdg_deg), 1),
        "spd_kt": round(spd_kt, 1),
    }
    return json.dumps(payload, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def run() -> int:
    parser = argparse.ArgumentParser(description="RecPlay UDP aircraft trajectory simulator")
    parser.add_argument("--address", default="239.1.1.1")
    parser.add_argument("--port", type=int, default=5000)
    parser.add_argument("--id", default="CCA1234")
    parser.add_argument("--rate-hz", type=float, default=10.0)
    parser.add_argument("--duration-sec", type=float, default=30.0)
    parser.add_argument("--lat", type=float, default=31.2)
    parser.add_argument("--lon", type=float, default=121.45)
    parser.add_argument("--alt-m", type=float, default=9500.0)
    parser.add_argument("--speed-kt", type=float, default=450.0)
    args = parser.parse_args()

    interval = 1.0 / args.rate_hz
    total_steps = max(1, int(args.duration_sec * args.rate_hz))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    try:
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
    except OSError:
        pass

    start = time.monotonic()
    lat = args.lat
    lon = args.lon
    alt_m = args.alt_m
    heading_deg = 82.0
    speed_kt = args.speed_kt

    print(
        f"Sending {total_steps} packets to {args.address}:{args.port} "
        f"at {args.rate_hz:.1f} Hz",
        flush=True,
    )

    for seq in range(total_steps):
        now = time.monotonic()
        elapsed_s = now - start
        progress = min(1.0, seq / max(1, total_steps - 1))

        heading_deg = 82.0 + 20.0 * math.sin(progress * math.pi * 1.4)
        alt_m = args.alt_m + 180.0 * math.sin(progress * math.pi * 2.0)
        speed_kt = args.speed_kt + 12.0 * math.sin(progress * math.pi * 0.75)

        packet = generate_packet(
            args.id,
            seq,
            int(elapsed_s * 1000.0),
            lat,
            lon,
            alt_m,
            heading_deg,
            speed_kt,
        )
        sock.sendto(packet, (args.address, args.port))

        speed_mps = speed_kt * 0.514444
        distance_m = speed_mps * interval
        lat_step = distance_m * math.cos(math.radians(heading_deg)) / meters_per_degree_lat()
        lon_step = distance_m * math.sin(math.radians(heading_deg)) / meters_per_degree_lon(lat)
        lat += lat_step
        lon += lon_step

        remaining = start + (seq + 1) * interval - time.monotonic()
        if remaining > 0:
            time.sleep(remaining)

    print("Simulation complete.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
