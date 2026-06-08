#!/usr/bin/env python3
"""
RecPlay flight data simulator — complete flight profile with 8 phases.

Sends JSON-over-UDP aircraft telemetry, fully compatible with
udp_ws_bridge.py, trajectory_viewer.html, and recv.py.

Usage examples:
  python tools/flight_sim.py                          # PEK→PVG, 5 min
  python tools/flight_sim.py --route PVG-CAN          # Shanghai→Guangzhou
  python tools/flight_sim.py --duration-sec 600       # 10-minute flight
  python tools/flight_sim.py --count 3                # 3 simultaneous aircraft
  python tools/flight_sim.py --list-routes            # show all routes
  python tools/flight_sim.py -v                       # verbose per-second log

Packet format (JSON, UTF-8, sent over UDP):
  {id, seq, t_ms, lat, lon, alt_m, hdg_deg, spd_kt,
   vspd_fpm, bank_deg, pitch_deg, phase, gear, flaps}
"""
from __future__ import annotations

import argparse
import json
import math
import socket
import sys
import threading
import time
from dataclasses import dataclass, field
from enum import Enum
from typing import List, Optional, Tuple


# ---------------------------------------------------------------------------
# Geo helpers
# ---------------------------------------------------------------------------

EARTH_R = 6_371_000.0


def haversine_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    lat1, lon1, lat2, lon2 = map(math.radians, (lat1, lon1, lat2, lon2))
    dlat = lat2 - lat1
    dlon = lon2 - lon1
    a = math.sin(dlat / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2) ** 2
    return 2 * EARTH_R * math.asin(math.sqrt(a))


def bearing_deg(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    lat1, lon1, lat2, lon2 = map(math.radians, (lat1, lon1, lat2, lon2))
    dlon = lon2 - lon1
    x = math.sin(dlon) * math.cos(lat2)
    y = math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(dlon)
    return math.degrees(math.atan2(x, y)) % 360.0


def move_point(lat: float, lon: float, heading_deg: float, distance_m: float) -> Tuple[float, float]:
    d = distance_m / EARTH_R
    lat_r = math.radians(lat)
    lon_r = math.radians(lon)
    hdg_r = math.radians(heading_deg)
    lat2 = math.asin(
        math.sin(lat_r) * math.cos(d) + math.cos(lat_r) * math.sin(d) * math.cos(hdg_r)
    )
    lon2 = lon_r + math.atan2(
        math.sin(hdg_r) * math.sin(d) * math.cos(lat_r),
        math.cos(d) - math.sin(lat_r) * math.sin(lat2),
    )
    return math.degrees(lat2), math.degrees(lon2)


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def clamp(x: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, x))


def smooth_step(t: float) -> float:
    t = clamp(t, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


# ---------------------------------------------------------------------------
# Performance constants (A320-class narrow-body)
# ---------------------------------------------------------------------------

ROTATE_SPD_KT        = 145.0
V2_SPD_KT            = 165.0
CRUISE_SPD_KT        = 450.0
DESCENT_SPD_KT       = 300.0
APPROACH_SPD_KT      = 155.0
TOUCHDOWN_SPD_KT     = 130.0
ROLLOUT_SPD_KT       = 20.0

CRUISE_ALT_M         = 10_000.0
TAKEOFF_ROLL_M       = 2_000.0
CLIMB_RATE_FPM       = 2_200.0
DESCENT_RATE_FPM     = 1_800.0


# ---------------------------------------------------------------------------
# Flight phases
# ---------------------------------------------------------------------------

class Phase(str, Enum):
    TAXI_OUT  = "TAXI_OUT"
    TAKEOFF   = "TAKEOFF"
    CLIMB     = "CLIMB"
    CRUISE    = "CRUISE"
    DESCENT   = "DESCENT"
    APPROACH  = "APPROACH"
    LANDING   = "LANDING"
    TAXI_IN   = "TAXI_IN"


# ---------------------------------------------------------------------------
# Route database
# ---------------------------------------------------------------------------

@dataclass
class Waypoint:
    name: str
    lat: float
    lon: float
    alt_m: float = 0.0


@dataclass
class Route:
    name: str
    description: str
    origin: Waypoint
    destination: Waypoint
    enroute: List[Waypoint] = field(default_factory=list)
    origin_rwy_hdg: float = 0.0
    dest_ils_hdg: float = 0.0


ROUTES: dict[str, Route] = {
    "PEK-PVG": Route(
        name="PEK-PVG",
        description="Beijing Capital → Shanghai Pudong  (1,070 km)",
        origin=Waypoint("ZBAA", 40.0799, 116.6031, 35.0),
        destination=Waypoint("ZSPD", 31.1443, 121.8083, 4.0),
        enroute=[
            Waypoint("WXI",   38.0, 117.8),
            Waypoint("DUMET", 35.0, 119.5),
        ],
        origin_rwy_hdg=36.0,
        dest_ils_hdg=135.0,
    ),
    "PVG-CAN": Route(
        name="PVG-CAN",
        description="Shanghai Pudong → Guangzhou Baiyun  (1,140 km)",
        origin=Waypoint("ZSPD", 31.1443, 121.8083, 4.0),
        destination=Waypoint("ZGGG", 23.3924, 113.2988, 15.0),
        enroute=[
            Waypoint("NIPOG", 29.0, 119.0),
            Waypoint("PIMAK", 26.0, 116.5),
        ],
        origin_rwy_hdg=170.0,
        dest_ils_hdg=220.0,
    ),
    "PEK-CAN": Route(
        name="PEK-CAN",
        description="Beijing Capital → Guangzhou Baiyun  (1,900 km)",
        origin=Waypoint("ZBAA", 40.0799, 116.6031, 35.0),
        destination=Waypoint("ZGGG", 23.3924, 113.2988, 15.0),
        enroute=[
            Waypoint("SUNRI", 37.0, 115.5),
            Waypoint("ELATO", 33.5, 114.2),
            Waypoint("IDUMA", 29.0, 113.8),
            Waypoint("IGOXU", 26.5, 113.5),
        ],
        origin_rwy_hdg=36.0,
        dest_ils_hdg=220.0,
    ),
    "HND-ZBAA": Route(
        name="HND-ZBAA",
        description="Tokyo Haneda → Beijing Capital  (2,100 km)",
        origin=Waypoint("RJTT", 35.5533, 139.7811, 7.0),
        destination=Waypoint("ZBAA", 40.0799, 116.6031, 35.0),
        enroute=[
            Waypoint("TEPAS", 36.5, 135.0),
            Waypoint("AROSA", 37.5, 130.0),
            Waypoint("ELGOR", 38.5, 125.0),
            Waypoint("KADMO", 39.0, 121.0),
        ],
        origin_rwy_hdg=340.0,
        dest_ils_hdg=216.0,
    ),
    "ICN-ZSPD": Route(
        name="ICN-ZSPD",
        description="Seoul Incheon → Shanghai Pudong  (870 km)",
        origin=Waypoint("RKSI", 37.4691, 126.4507, 7.0),
        destination=Waypoint("ZSPD", 31.1443, 121.8083, 4.0),
        enroute=[
            Waypoint("MOTAT", 35.5, 124.5),
            Waypoint("DOLPU", 33.5, 122.8),
        ],
        origin_rwy_hdg=160.0,
        dest_ils_hdg=45.0,
    ),
}

_DEFAULT_CALLSIGNS: dict[str, list[str]] = {
    "PEK-PVG":  ["CCA1234", "CSN8888", "CES5150"],
    "PVG-CAN":  ["CZ3001",  "MU5100",  "SC1234"],
    "PEK-CAN":  ["CZ3130",  "CX8888",  "KE982"],
    "HND-ZBAA": ["CA1112",  "NH9180",  "MU7788"],
    "ICN-ZSPD": ["KE821",   "OZ341",   "MU503"],
}


# ---------------------------------------------------------------------------
# Flight profile
# ---------------------------------------------------------------------------

@dataclass
class AircraftState:
    lat: float
    lon: float
    alt_m: float
    hdg_deg: float
    spd_kt: float
    vspeed_fpm: float
    bank_deg: float
    pitch_deg: float
    phase: Phase
    gear_down: bool
    flaps: int


class FlightProfile:
    """
    Generates AircraftState for any simulation time t in [0, total_s].
    Phase durations are proportionally scaled to fit total_s.
    """

    _BASE_TAXI_OUT   = 30.0
    _BASE_TAKEOFF    = 40.0
    _BASE_CLIMB      = 90.0
    _BASE_CRUISE_MIN = 60.0
    _BASE_DESCENT    = 90.0
    _BASE_APPROACH   = 60.0
    _BASE_LANDING    = 35.0
    _BASE_TAXI_IN    = 30.0
    _BASE_OVERHEAD   = (_BASE_TAXI_OUT + _BASE_TAKEOFF + _BASE_CLIMB +
                        _BASE_DESCENT + _BASE_APPROACH + _BASE_LANDING + _BASE_TAXI_IN)

    def __init__(self, route: Route, total_s: float) -> None:
        self.route = route
        self.total_s = total_s
        minimum = self._BASE_OVERHEAD + self._BASE_CRUISE_MIN
        if total_s < minimum:
            scale = total_s / minimum
        else:
            scale = 1.0

        self._taxi_out  = self._BASE_TAXI_OUT  * scale
        self._takeoff   = self._BASE_TAKEOFF   * scale
        self._climb     = self._BASE_CLIMB     * scale
        self._descent   = self._BASE_DESCENT   * scale
        self._approach  = self._BASE_APPROACH  * scale
        self._landing   = self._BASE_LANDING   * scale
        self._taxi_in   = self._BASE_TAXI_IN   * scale
        overhead = (self._taxi_out + self._takeoff + self._climb +
                    self._descent + self._approach + self._landing + self._taxi_in)
        self._cruise = max(self._BASE_CRUISE_MIN * scale, total_s - overhead)

        self._t_takeoff  = self._taxi_out
        self._t_climb    = self._t_takeoff  + self._takeoff
        self._t_cruise   = self._t_climb    + self._climb
        self._t_descent  = self._t_cruise   + self._cruise
        self._t_approach = self._t_descent  + self._descent
        self._t_landing  = self._t_approach + self._approach
        self._t_taxi_in  = self._t_landing  + self._landing

        self._build_route()

    def _build_route(self) -> None:
        wpts = [self.route.origin] + self.route.enroute + [self.route.destination]
        self._wpts = wpts
        dists: list[float] = []
        total = 0.0
        for i in range(1, len(wpts)):
            d = haversine_m(wpts[i - 1].lat, wpts[i - 1].lon, wpts[i].lat, wpts[i].lon)
            dists.append(d)
            total += d
        self._dists = dists
        self._total_dist = total

    def _route_pos(self, frac: float) -> Tuple[float, float, float]:
        """Return (lat, lon, hdg) at route fraction 0..1."""
        frac = clamp(frac, 0.0, 1.0)
        if self._total_dist == 0:
            w = self._wpts[0]
            return w.lat, w.lon, self.route.origin_rwy_hdg
        target = frac * self._total_dist
        acc = 0.0
        for i, d in enumerate(self._dists):
            if acc + d >= target or i == len(self._dists) - 1:
                sf = clamp((target - acc) / d, 0.0, 1.0) if d > 0 else 0.0
                lat = lerp(self._wpts[i].lat,  self._wpts[i + 1].lat,  sf)
                lon = lerp(self._wpts[i].lon,  self._wpts[i + 1].lon,  sf)
                hdg = bearing_deg(self._wpts[i].lat, self._wpts[i].lon,
                                  self._wpts[i + 1].lat, self._wpts[i + 1].lon)
                return lat, lon, hdg
            acc += d
        w = self._wpts[-1]
        return w.lat, w.lon, self.route.dest_ils_hdg

    def state_at(self, t: float) -> AircraftState:  # noqa: C901
        t = clamp(t, 0.0, self.total_s)
        o = self.route.origin
        d = self.route.destination

        # --- TAXI_OUT --------------------------------------------------
        if t < self._t_takeoff:
            f = t / self._taxi_out if self._taxi_out > 0 else 0.0
            return AircraftState(
                lat=o.lat, lon=o.lon, alt_m=o.alt_m,
                hdg_deg=self.route.origin_rwy_hdg,
                spd_kt=lerp(0.0, 20.0, f),
                vspeed_fpm=0.0, bank_deg=0.0, pitch_deg=0.0,
                phase=Phase.TAXI_OUT, gear_down=True, flaps=2,
            )

        # --- TAKEOFF ---------------------------------------------------
        if t < self._t_climb:
            f = (t - self._t_takeoff) / self._takeoff
            rotate_f = clamp((f - 0.6) / 0.4, 0.0, 1.0)
            if f < 0.6:
                spd = lerp(0.0, ROTATE_SPD_KT, smooth_step(f / 0.6))
            else:
                spd = lerp(ROTATE_SPD_KT, V2_SPD_KT, smooth_step(rotate_f))
            alt = o.alt_m + lerp(0.0, 50.0, smooth_step(rotate_f))
            roll_dist = TAKEOFF_ROLL_M * smooth_step(clamp(f / 0.6, 0.0, 1.0))
            lat, lon = move_point(o.lat, o.lon, self.route.origin_rwy_hdg, roll_dist)
            if f > 0.6:
                lat, lon = move_point(lat, lon, self.route.origin_rwy_hdg,
                                      500.0 * smooth_step(rotate_f))
            return AircraftState(
                lat=lat, lon=lon, alt_m=alt,
                hdg_deg=self.route.origin_rwy_hdg,
                spd_kt=spd,
                vspeed_fpm=lerp(0.0, 2000.0, smooth_step(rotate_f)),
                bank_deg=0.0, pitch_deg=lerp(0.0, 12.0, smooth_step(rotate_f)),
                phase=Phase.TAKEOFF,
                gear_down=rotate_f < 0.5, flaps=3,
            )

        # --- CLIMB -----------------------------------------------------
        if t < self._t_cruise:
            f = (t - self._t_climb) / self._climb
            sp = smooth_step(f)
            alt = lerp(o.alt_m + 50.0, CRUISE_ALT_M, sp)
            spd = lerp(V2_SPD_KT + 30.0, CRUISE_SPD_KT, sp)
            vspeed = lerp(2000.0, 500.0, sp) if f < 0.9 else lerp(500.0, 0.0, (f - 0.9) / 0.1)
            lat, lon, hdg = self._route_pos(f * 0.10)
            return AircraftState(
                lat=lat, lon=lon, alt_m=alt, hdg_deg=hdg, spd_kt=spd,
                vspeed_fpm=vspeed,
                bank_deg=lerp(0.0, 15.0, math.sin(f * math.pi)),
                pitch_deg=lerp(12.0, 4.0, sp),
                phase=Phase.CLIMB,
                gear_down=False, flaps=1 if f < 0.3 else 0,
            )

        # --- CRUISE ----------------------------------------------------
        if t < self._t_descent:
            f = (t - self._t_cruise) / self._cruise
            route_frac = lerp(0.10, 0.90, f)
            lat, lon, hdg = self._route_pos(route_frac)
            # Look-ahead bank for turns
            _, _, hdg_next = self._route_pos(min(1.0, route_frac + 0.02))
            hdg_delta = ((hdg_next - hdg + 180) % 360) - 180
            bank = clamp(hdg_delta * 1.5, -28.0, 28.0)
            return AircraftState(
                lat=lat, lon=lon,
                alt_m=CRUISE_ALT_M + 120.0 * math.sin(f * math.pi * 2.1),
                hdg_deg=hdg,
                spd_kt=CRUISE_SPD_KT + 15.0 * math.sin(f * math.pi * 3.7),
                vspeed_fpm=50.0 * math.sin(f * math.pi * 5.3),
                bank_deg=bank, pitch_deg=2.5,
                phase=Phase.CRUISE, gear_down=False, flaps=0,
            )

        # --- DESCENT ---------------------------------------------------
        if t < self._t_approach:
            f = (t - self._t_descent) / self._descent
            sp = smooth_step(f)
            lat, lon, hdg = self._route_pos(lerp(0.90, 0.95, f))
            return AircraftState(
                lat=lat, lon=lon,
                alt_m=lerp(CRUISE_ALT_M, 1_500.0, sp),
                hdg_deg=hdg,
                spd_kt=lerp(CRUISE_SPD_KT, DESCENT_SPD_KT, sp),
                vspeed_fpm=lerp(0.0, -DESCENT_RATE_FPM, smooth_step(f)),
                bank_deg=0.0, pitch_deg=lerp(2.5, -3.0, sp),
                phase=Phase.DESCENT, gear_down=False, flaps=0,
            )

        # --- APPROACH --------------------------------------------------
        if t < self._t_landing:
            f = (t - self._t_approach) / self._approach
            sp = smooth_step(f)
            lat, lon, _ = self._route_pos(lerp(0.95, 0.99, f))
            return AircraftState(
                lat=lat, lon=lon,
                alt_m=lerp(1_500.0, d.alt_m + 30.0, sp),
                hdg_deg=self.route.dest_ils_hdg,
                spd_kt=lerp(DESCENT_SPD_KT, APPROACH_SPD_KT, sp),
                vspeed_fpm=lerp(-DESCENT_RATE_FPM, -700.0, sp),
                bank_deg=lerp(0.0, 2.0, math.sin(f * math.pi * 2.3)) * (1 - f),
                pitch_deg=lerp(-3.0, -3.5, sp),
                phase=Phase.APPROACH,
                gear_down=f > 0.4, flaps=3 if f > 0.3 else 1,
            )

        # --- LANDING ---------------------------------------------------
        if t < self._t_taxi_in:
            f = (t - self._t_landing) / self._landing
            sp = smooth_step(f)
            flare_f = clamp(f / 0.3, 0.0, 1.0)
            alt = lerp(d.alt_m + 30.0, d.alt_m, smooth_step(flare_f))
            rollout_dist = lerp(2_500.0, 0.0, sp)
            lat, lon = move_point(d.lat, d.lon,
                                  (self.route.dest_ils_hdg + 180) % 360,
                                  rollout_dist)
            return AircraftState(
                lat=lat, lon=lon, alt_m=alt,
                hdg_deg=self.route.dest_ils_hdg,
                spd_kt=lerp(APPROACH_SPD_KT, ROLLOUT_SPD_KT, sp),
                vspeed_fpm=lerp(-700.0, 0.0, smooth_step(flare_f)),
                bank_deg=0.0, pitch_deg=lerp(-3.5, 0.0, smooth_step(flare_f)),
                phase=Phase.LANDING, gear_down=True, flaps=4,
            )

        # --- TAXI_IN ---------------------------------------------------
        f = clamp((t - self._t_taxi_in) / self._taxi_in, 0.0, 1.0) if self._taxi_in > 0 else 1.0
        return AircraftState(
            lat=d.lat, lon=d.lon, alt_m=d.alt_m,
            hdg_deg=self.route.dest_ils_hdg,
            spd_kt=lerp(ROLLOUT_SPD_KT, 0.0, f),
            vspeed_fpm=0.0, bank_deg=0.0, pitch_deg=0.0,
            phase=Phase.TAXI_IN, gear_down=True, flaps=0,
        )


# ---------------------------------------------------------------------------
# Packet serialisation
# ---------------------------------------------------------------------------

def make_packet(aircraft_id: str, seq: int, elapsed_ms: int, s: AircraftState) -> bytes:
    d = {
        "id":        aircraft_id,
        "seq":       seq,
        "t_ms":      elapsed_ms,
        "lat":       round(s.lat, 6),
        "lon":       round(s.lon, 6),
        "alt_m":     round(s.alt_m, 1),
        "hdg_deg":   round(s.hdg_deg % 360.0, 1),
        "spd_kt":    round(s.spd_kt, 1),
        "vspd_fpm":  round(s.vspeed_fpm, 0),
        "bank_deg":  round(s.bank_deg, 1),
        "pitch_deg": round(s.pitch_deg, 1),
        "phase":     s.phase.value,
        "gear":      1 if s.gear_down else 0,
        "flaps":     s.flaps,
    }
    return json.dumps(d, separators=(",", ":")).encode("utf-8")


# ---------------------------------------------------------------------------
# Aircraft runner (one thread per aircraft)
# ---------------------------------------------------------------------------

def _run_aircraft(
    aircraft_id: str,
    route: Route,
    duration_s: float,
    rate_hz: float,
    address: str,
    port: int,
    start_offset_s: float = 0.0,
    verbose: bool = False,
) -> None:
    profile = FlightProfile(route, duration_s)
    interval = 1.0 / rate_hz
    total_steps = max(1, int(duration_s * rate_hz))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    try:
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
    except OSError:
        pass

    if start_offset_s > 0:
        time.sleep(start_offset_s)

    wall_start = time.monotonic()
    for seq in range(total_steps):
        t_sim = seq * interval
        s = profile.state_at(t_sim)
        elapsed_ms = int((time.monotonic() - wall_start) * 1000)
        sock.sendto(make_packet(aircraft_id, seq, elapsed_ms, s), (address, port))

        if verbose and seq % max(1, int(rate_hz)) == 0:
            print(
                f"  {aircraft_id} #{seq:4d} {s.phase.value:10s} "
                f"lat={s.lat:8.4f} lon={s.lon:8.4f} "
                f"alt={s.alt_m:6.0f}m spd={s.spd_kt:5.1f}kt",
                flush=True,
            )

        target = wall_start + (seq + 1) * interval
        remaining = target - time.monotonic()
        if remaining > 0:
            time.sleep(remaining)

    if verbose:
        print(f"  {aircraft_id}: complete.", flush=True)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def run() -> int:
    parser = argparse.ArgumentParser(
        description="RecPlay flight data simulator — complete flight profile with 8 phases.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument("--address", default="239.1.1.1")
    parser.add_argument("--port", type=int, default=5000)
    parser.add_argument("--route", default="PEK-PVG",
                        help="Route name (default: PEK-PVG)")
    parser.add_argument("--count", type=int, default=1,
                        help="Simultaneous aircraft count (default: 1)")
    parser.add_argument("--duration-sec", type=float, default=300.0,
                        help="Total flight duration in seconds (default: 300)")
    parser.add_argument("--rate-hz", type=float, default=5.0,
                        help="Packet rate in Hz (default: 5)")
    parser.add_argument("--stagger-sec", type=float, default=20.0,
                        help="Start delay between aircraft (default: 20s)")
    parser.add_argument("--id", default="",
                        help="Custom aircraft ID (prefix when --count > 1)")
    parser.add_argument("--list-routes", action="store_true")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    if args.list_routes:
        print("Available routes:")
        for key, r in ROUTES.items():
            print(f"  {key:12s}  {r.description}")
        return 0

    if args.route not in ROUTES:
        print(f"Error: unknown route '{args.route}'", file=sys.stderr)
        print(f"Available: {', '.join(ROUTES)}", file=sys.stderr)
        return 1

    route = ROUTES[args.route]
    id_pool = _DEFAULT_CALLSIGNS.get(args.route, [f"AC{i:04d}" for i in range(10)])
    if args.id:
        if args.count > 1:
            aircraft_ids = [f"{args.id}{i + 1:02d}" for i in range(args.count)]
        else:
            aircraft_ids = [args.id]
    else:
        aircraft_ids = [id_pool[i % len(id_pool)] for i in range(args.count)]

    pkt_count = int(args.duration_sec * args.rate_hz)
    print(f"Route:    {route.name}  —  {route.description}")
    print(f"Aircraft: {', '.join(aircraft_ids)}")
    print(f"Duration: {args.duration_sec:.0f}s  Rate: {args.rate_hz:.1f} Hz  "
          f"Packets/ac: {pkt_count}")
    print(f"Target:   udp://{args.address}:{args.port}")
    print()

    if args.count == 1:
        _run_aircraft(
            aircraft_ids[0], route, args.duration_sec,
            args.rate_hz, args.address, args.port,
            verbose=args.verbose,
        )
    else:
        threads = []
        for i in range(args.count):
            offset = i * args.stagger_sec
            t = threading.Thread(
                target=_run_aircraft,
                args=(aircraft_ids[i], route, args.duration_sec,
                      args.rate_hz, args.address, args.port),
                kwargs={"start_offset_s": offset, "verbose": args.verbose},
                daemon=True,
            )
            threads.append(t)
            t.start()
            print(f"Started {aircraft_ids[i]} (offset +{offset:.0f}s)", flush=True)
        for t in threads:
            t.join()

    print("Simulation complete.")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
