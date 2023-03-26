#!/usr/bin/env python3
import csv
import argparse
from typing import List
from dataclasses import dataclass, fields
from collections import OrderedDict


def parse_args():
    a = argparse.ArgumentParser(
        description="generate spawn.ini",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    a.add_argument("-i", "--input-path", help="player config path", type=str)
    a.add_argument(
        "-f",
        "--frame-send-rate",
        help="FrameSendRate param",
        type=int,
        default=1,
    )
    a.add_argument(
        "-pr",
        "--protocol-version",
        help="protocol version",
        type=int,
        default=0,
    )
    a.add_argument(
        "-g",
        "--game-mode",
        help="game mode",
        choices=["yr", "ra2"],
        default="yr",
        type=str,
    )
    a.add_argument("-s", "--speed", help="Game speed", type=int, default=0)
    a.add_argument("-p", "--player-id", help="Current player id", type=int)
    a.add_argument(
        "-t", "--tunnel-address", help="Tunnel address", default="0.0.0.0"
    )
    a.add_argument("-tp", "--tunnel-port", help="Tunnel port", default=50000)
    return a.parse_args()


@dataclass
class PlayerSetting:
    name: str
    color: int
    side: int
    is_host: str
    is_observer: str
    ai_difficulty: int
    port: int


@dataclass
class GlobalSettings:
    Credits: int
    FrameSendRate: int
    GameMode: int
    GameSpeed: int
    MCVRedeploy: bool
    Protocol: int
    Ra2Mode: bool
    Scenario: str
    Seed: int
    ShortGame: bool
    SidebarHack: bool
    SuperWeapons: bool
    GameID: int
    Bases: str
    UnitCount: int
    Host: bool
    Port: int
    UIGameMode: str


@dataclass
class TunnelSettings:
    Ip: str
    Port: int


def parse_players(input_path) -> List[PlayerSetting]:
    fld = {f.name: f.type for f in fields(PlayerSetting)}
    res = []
    with open(input_path, "r") as f:
        R = csv.DictReader(f, delimiter="\t")
        for e in R:
            entries = {k: fld[k](v) for k, v in e.items()}
            res.append(PlayerSetting(**entries))
    return res


def get_main_settings(
    s: GlobalSettings, p: PlayerSetting, p_all: List[PlayerSetting]
):
    ai_players = len([x for x in p_all if x.ai_difficulty != -1])
    player_count = len(p_all) - ai_players
    entries = OrderedDict(
        [
            ("AIPlayers", ai_players),
            ("PlayerCount", player_count),
            ("IsSpectator", p.is_observer),
            ("Name", p.name),
            ("Color", p.color),
            ("Side", p.side),
        ]
    )
    for f in fields(s):
        entries[f.name] = getattr(s, f.name)
    entries["Host"] = p.is_host
    entries["Port"] = p.port
    return "\n".join(["[Settings]"] + [f"{k}={v}" for k, v in entries.items()])


def main():
    args = parse_args()
    entries = None
    with open(args.input_path, "r") as f:
        R = csv.DictReader(f, delimiter="\t")
        entries = [x for x in R]

    ai_section = []
    others_section = []
    for i, e in enumerate(x for x in entries if x["ai_difficulty"] != "-1"):
        ai_section.append((i, e))

    for i, e in (
        (j, x)
        for j, x in enumerate(entries)
        if x["ai_difficulty"] == "-1" and j != args.player_id
    ):
        others_section.append((i, e))

    all_players = parse_players(args.input_path)
    G = GlobalSettings(
        Credits=10000,
        FrameSendRate=args.frame_send_rate,
        GameMode=1,
        GameSpeed=args.speed,
        MCVRedeploy=True,
        Protocol=args.protocol_version,
        Ra2Mode=args.game_mode == "ra2",
        Scenario="spawnmap.ini",
        Seed=123,
        ShortGame=True,
        SidebarHack="Yes",
        SuperWeapons=True,
        GameID=12850,
        Bases="Yes",
        UnitCount=0,
        Host=False,
        Port=-1,
        UIGameMode="Battle",
    )
    T = TunnelSettings(Ip=args.tunnel_address, Port=args.tunnel_port)

    sections = []
    # emit main settings
    p = all_players[args.player_id]
    sections.append(get_main_settings(G, p, all_players))

    # emit ai entries
    if ai_section:
        sections.append(
            "\n".join(
                ["[HouseHandicaps]"]
                + [
                    "Multi{}={}".format(i + 1, e["ai_difficulty"])
                    for i, e in ai_section
                ]
            )
        )
        sections.append(
            "\n".join(
                ["[HouseCountries]"]
                + ["Multi{}={}".format(i + 1, e["side"]) for i, e in ai_section]
            )
        )
        sections.append(
            "\n".join(
                ["[HouseColors]"]
                + [
                    "Multi{}={}".format(i + 1, e["color"])
                    for i, e in ai_section
                ]
            )
        )

    # emit other players
    keymap = OrderedDict(
        [
            ("Name", "name"),
            ("Side", "side"),
            ("IsSpectator", "is_observer"),
            ("Port", "port"),
            ("Color", "color"),
        ]
    )
    if others_section:
        for i, e in others_section:
            ee = [f"{k}={e[v]}" for k, v in keymap.items()] + [f"Ip={T.Ip}"]
            entries = "\n".join(ee)
            sections.append("\n".join([f"[Other{i+1}]", entries]))

    sections.append(
        "\n".join(
            ["[SpawnLocations]"]
            + ["Multi{}={}".format(i + 1, i) for i, _ in enumerate(all_players)]
        )
    )

    if others_section:
        entries = "\n".join(f"{f.name}={getattr(T, f.name)}" for f in fields(T))
        # for f in fields(T):
        #         entries[f.name] = getattr(s, f.name)
        sections.append("\n".join(["[Tunnel]"] + [entries]))
    print("\n\n".join(sections))


if __name__ == "__main__":
    main()
