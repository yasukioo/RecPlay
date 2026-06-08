#!/usr/bin/env python3
"""
RecPlay UDP data receiver — validates replayed packets and reports statistics.

Joins the same multicast group used by flight_sim.py and the RecPlay UDP
protocol service, then reports live packet stats and, optionally, writes
a CSV log for offline analysis.

Usage:
  python tools/recv.py                          # listen on default multicast
  python tools/recv.py --csv flight.csv         # also write CSV log
  python tools/recv.py --duration-sec 120       # auto-exit after 2 min
  python tools/recv.py --address 127.0.0.1      # unicast fallback
  python tools/recv.py --verbose                # print every packet

What it checks:
  - Sequence number continuity per aircraft (gaps = dropped packets)
  - Average and peak packet rate
  - Timing jitter (wall-clock interval vs t_ms delta)
  - Altitude and speed range per aircraft
  - Flight phases actually received
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import socket
import sys
import time
from typing import Optional


# ---------------------------------------------------------------------------
# Per-aircraft statistics accumulator
# ---------------------------------------------------------------------------

class AircraftStats:
    def __init__(self, aircraft_id: str) -> None:
        self.id = aircraft_id
        self.count = 0
        self.last_seq: Optional[int] = None
        self.seq_gaps = 0
        self.alt_min = math.inf
        self.alt_max = -math.inf
        self.spd_min = math.inf
        self.spd_max = -math.inf
        self.phases_seen: set[str] = set()
        self.jitter_samples: list[float] = []
        self.first_wall: Optional[float] = None
        self.last_wall: Optional[float] = None
        self.last_t_ms: Optional[int] = None

    def update(self, pkt: dict, wall: float) -> None:
        self.count += 1
        if self.first_wall is None:
            self.first_wall = wall
        self.last_wall = wall

        seq = pkt.get("seq")
        if seq is not None and self.last_seq is not None:
            gap = seq - self.last_seq - 1
            if gap > 0:
                self.seq_gaps += gap
        self.last_seq = seq

        alt = pkt.get("alt_m")
        if alt is not None:
            self.alt_min = min(self.alt_min, float(alt))
            self.alt_max = max(self.alt_max, float(alt))

        spd = pkt.get("spd_kt")
        if spd is not None:
            self.spd_min = min(self.spd_min, float(spd))
            self.spd_max = max(self.spd_max, float(spd))

        phase = pkt.get("phase")
        if phase:
            self.phases_seen.add(str(phase))

        t_ms = pkt.get("t_ms")
        if t_ms is not None and self.last_t_ms is not None:
            expected = (int(t_ms) - int(self.last_t_ms)) / 1000.0
            actual = wall - (self.last_wall or wall)
            if 0 < expected < 5.0:
                self.jitter_samples.append(abs(actual - expected) * 1000.0)
                if len(self.jitter_samples) > 200:
                    self.jitter_samples.pop(0)
        self.last_t_ms = t_ms

    @property
    def elapsed_s(self) -> float:
        if self.first_wall is None or self.last_wall is None:
            return 0.0
        return self.last_wall - self.first_wall

    @property
    def avg_rate_hz(self) -> float:
        if self.elapsed_s <= 0 or self.count <= 1:
            return 0.0
        return (self.count - 1) / self.elapsed_s

    @property
    def avg_jitter_ms(self) -> float:
        return sum(self.jitter_samples) / len(self.jitter_samples) if self.jitter_samples else 0.0

    @property
    def p95_jitter_ms(self) -> float:
        if len(self.jitter_samples) < 5:
            return 0.0
        s = sorted(self.jitter_samples)
        return s[int(len(s) * 0.95)]


# ---------------------------------------------------------------------------
# Receiver
# ---------------------------------------------------------------------------

_CSV_FIELDS = [
    "wall_time_s", "id", "seq", "t_ms", "lat", "lon", "alt_m",
    "hdg_deg", "spd_kt", "vspd_fpm", "phase", "bank_deg", "pitch_deg", "gear", "flaps",
]


def _is_multicast(address: str) -> bool:
    try:
        return 224 <= int(address.split(".")[0]) <= 239
    except (ValueError, IndexError):
        return False


def _join_multicast(sock: socket.socket, address: str) -> None:
    mreq = socket.inet_aton(address) + socket.inet_aton("0.0.0.0")
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)


def _print_summary(per_aircraft: dict[str, AircraftStats], total: int, errors: int) -> None:
    sep = "=" * 72
    print(f"\n{sep}", flush=True)
    print(
        f"SUMMARY  —  {len(per_aircraft)} aircraft  |  "
        f"{total} total packets  |  {errors} decode errors",
        flush=True,
    )
    print(sep, flush=True)
    for ac_id, s in sorted(per_aircraft.items()):
        gaps_warn = "  *** SEQUENCE GAPS ***" if s.seq_gaps > 0 else ""
        print(f"\n  [{ac_id}]{gaps_warn}", flush=True)
        print(f"    Packets   : {s.count}  seq_gaps: {s.seq_gaps}", flush=True)
        print(f"    Rate      : {s.avg_rate_hz:.2f} Hz   duration: {s.elapsed_s:.1f} s", flush=True)
        if s.alt_min != math.inf:
            print(f"    Altitude  : {s.alt_min:.0f} – {s.alt_max:.0f} m", flush=True)
        if s.spd_min != math.inf:
            print(f"    Speed     : {s.spd_min:.0f} – {s.spd_max:.0f} kt", flush=True)
        if s.phases_seen:
            ordered = ["TAXI_OUT", "TAKEOFF", "CLIMB", "CRUISE", "DESCENT",
                       "APPROACH", "LANDING", "TAXI_IN"]
            seen = [p for p in ordered if p in s.phases_seen]
            extra = s.phases_seen - set(ordered)
            seen += sorted(extra)
            print(f"    Phases    : {' → '.join(seen)}", flush=True)
        print(f"    Jitter    : avg {s.avg_jitter_ms:.1f} ms   p95 {s.p95_jitter_ms:.1f} ms",
              flush=True)
    print(f"\n{sep}", flush=True)


def run() -> int:
    parser = argparse.ArgumentParser(
        description="RecPlay UDP data receiver — validates replayed packets.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument("--address", default="239.1.1.1",
                        help="Multicast/unicast address (default: 239.1.1.1)")
    parser.add_argument("--port", type=int, default=5000,
                        help="UDP port (default: 5000)")
    parser.add_argument("--csv", metavar="PATH",
                        help="Write received packets to CSV file")
    parser.add_argument("--duration-sec", type=float,
                        help="Auto-exit after N seconds")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Print every packet")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(("0.0.0.0", args.port))
    except OSError:
        sock.bind(("", args.port))

    if _is_multicast(args.address):
        try:
            _join_multicast(sock, args.address)
            print(f"Joined multicast {args.address}:{args.port}", flush=True)
        except OSError as exc:
            print(f"Warning: multicast join failed ({exc}); continuing anyway.", flush=True)
    else:
        print(f"Listening on {args.address}:{args.port} (unicast)", flush=True)

    sock.settimeout(1.0)

    csv_file = None
    csv_writer: Optional[csv.DictWriter] = None
    if args.csv:
        csv_file = open(args.csv, "w", newline="", encoding="utf-8")
        csv_writer = csv.DictWriter(csv_file, fieldnames=_CSV_FIELDS, extrasaction="ignore")
        csv_writer.writeheader()
        print(f"CSV log → {args.csv}", flush=True)

    per_aircraft: dict[str, AircraftStats] = {}
    total_packets = 0
    total_errors = 0
    start_wall = time.monotonic()
    last_status = start_wall

    print("Waiting for packets… (Ctrl+C to stop)\n", flush=True)

    try:
        while True:
            now = time.monotonic()
            if args.duration_sec and (now - start_wall) >= args.duration_sec:
                print(f"\nDuration limit {args.duration_sec:.0f}s reached.", flush=True)
                break

            try:
                data, _ = sock.recvfrom(65_535)
            except socket.timeout:
                if time.monotonic() - last_status > 5.0:
                    elapsed = time.monotonic() - start_wall
                    print(f"… {elapsed:.0f}s elapsed, {total_packets} packets received …",
                          flush=True)
                    last_status = time.monotonic()
                continue

            recv_wall = time.monotonic()
            total_packets += 1

            try:
                pkt = json.loads(data.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                total_errors += 1
                if total_errors <= 5:
                    print(f"[DECODE ERROR] {exc}: {data[:80]}", flush=True)
                continue

            ac_id = str(pkt.get("id", "UNKNOWN"))
            if ac_id not in per_aircraft:
                per_aircraft[ac_id] = AircraftStats(ac_id)
                print(f"[NEW] Aircraft: {ac_id}", flush=True)

            s = per_aircraft[ac_id]
            s.update(pkt, recv_wall)

            if csv_writer:
                row = dict(pkt)
                row["wall_time_s"] = round(recv_wall - start_wall, 3)
                csv_writer.writerow(row)

            if args.verbose:
                print(
                    f"  [{ac_id}] #{pkt.get('seq','?'):4}  "
                    f"{pkt.get('phase','?'):10s}  "
                    f"lat={pkt.get('lat', 0):8.4f}  lon={pkt.get('lon', 0):8.4f}  "
                    f"alt={pkt.get('alt_m', 0):6.0f}m  spd={pkt.get('spd_kt', 0):5.1f}kt",
                    flush=True,
                )
            elif s.count % 50 == 0:
                gaps_str = f"  GAPS:{s.seq_gaps}" if s.seq_gaps > 0 else ""
                print(
                    f"[{ac_id}] #{s.count:4d}  {pkt.get('phase', '?'):10s}  "
                    f"lat={pkt.get('lat', 0):8.4f}  lon={pkt.get('lon', 0):8.4f}  "
                    f"alt={pkt.get('alt_m', 0):6.0f}m  spd={pkt.get('spd_kt', 0):5.1f}kt"
                    f"{gaps_str}",
                    flush=True,
                )
                last_status = time.monotonic()

    except KeyboardInterrupt:
        print("\nInterrupted.", flush=True)
    finally:
        sock.close()
        if csv_file:
            csv_file.close()
        _print_summary(per_aircraft, total_packets, total_errors)

    return 0 if total_errors == 0 else 1


if __name__ == "__main__":
    raise SystemExit(run())
