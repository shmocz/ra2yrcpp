#!/usr/bin/env python3

import json
import functools
import tempfile
import traceback
import datetime
from enum import Enum
import re
import time
import atexit
import logging
import subprocess
from pathlib import Path
import shutil
import argparse
import sys
import os
from dataclasses import dataclass, fields, field

NAME_SPAWNER = "gamemd-spawn.exe"
NAME_SPAWNER_PATCHED = "gamemd-spawn-ra2yrcpp.exe"
NAME_SPAWNER_SYRINGE = "gamemd-spawn-syr.exe"

FILE_PATHS = f"""\
BINKW32.DLL
spawner.xdp
ra2.mix
ra2md.mix
theme.mix
thememd.mix
langmd.mix
language.mix
expandmd01.mix
mapsmd03.mix
maps02.mix
maps01.mix
Ra2.tlb
INI
Maps
RA2.INI
RA2MD.ini
ddraw.ini
spawner2.xdp
Blowfish.dll
ddraw.dll
Syringe.exe
cncnet5.dll
{NAME_SPAWNER}\
"""


LIB_PATHS = f"""\
{NAME_SPAWNER_PATCHED}
libgcc_s_dw2-1.dll
libra2yrcpp.dll
libstdc++-6.dll
libwinpthread-1.dll
zlib1.dll\
"""


def setup_logging(level=logging.INFO):
    FORMAT = "[%(levelname)s] %(asctime)s %(module)s.%(filename)s:%(lineno)d: %(message)s"
    logging.basicConfig(level=level, format=FORMAT)
    start_time = datetime.datetime.now().isoformat()
    level_name = logging.getLevelName(level)
    logging.info("Logging started at: %s, level=%s", start_time, level_name)


def mklink(dst, src):
    if os.path.islink(dst):
        os.unlink(dst)
    os.symlink(src, dst)


def read_file(p):
    with open(p, "r") as f:
        return f.read()


class ProtocolVersion(Enum):
    zero = 0
    compress = 2


# TODO: put to pyra2yr
class Color(Enum):
    Yellow = 0
    Red = 1
    Blue = 2
    Green = 3
    Orange = 4
    Teal = 5
    Purple = 6
    Pink = 7


@dataclass
class PlayerEntry:
    name: str
    color: Color
    side: int
    location: int
    index: int
    is_host: bool = False
    is_observer: bool = False
    ai_difficulty: int = -1
    port: int = 0

    def __post_init__(self):
        if self.port <= 0:
            self.port = 13360 + self.index
        if not isinstance(self.color, Color):
            self.color = Color[self.color]


@dataclass
class ConfigFile:
    """Class that maps directly to the JSON configuration."""

    map_path: str
    unit_count: int
    start_credits: int
    seed: int
    ra2_mode: bool
    short_game: bool
    superweapons: bool
    protocol: ProtocolVersion
    tunnel: str
    port: int
    game_speed: int = 0
    frame_send_rate: int = 1
    crates: bool = False
    mcv_redeploy: bool = True
    allies_allowed: bool = True
    multi_engineer: bool = False
    bridges_destroyable: bool = True
    build_off_ally: bool = True
    players: list[PlayerEntry] = field(default_factory=list)

    def __post_init__(self):
        for k in ["color", "location", "name"]:
            if len(set(getattr(p, k) for p in self.players)) != len(
                self.players
            ):
                raise RuntimeError(f'Duplicate player attribute: "{k}"')

    @classmethod
    def load(cls, path):
        fld = {f.name: f.type for f in fields(cls)}
        args = {}
        d = json.loads(read_file(path))
        for k, v in d.items():
            if k == "players":
                args[k] = [PlayerEntry(index=i, **x) for (i, x) in enumerate(v)]
            elif k == "protocol":
                args[k] = ProtocolVersion[v]
            else:
                args[k] = fld[k](v)
        return cls(**args)

    def kv_to_string(self, name, x):
        return f"[{name}]\n" + "\n".join(f"{k}={v}" for k, v in x)

    def player_values(self, p: PlayerEntry):
        kmap = [
            ("Name", "name"),
            ("Side", "side"),
            ("IsSpectator", "is_observer"),
            ("Port", "port"),
        ]
        return [(k, getattr(p, v)) for k, v in kmap] + [
            ("Color", p.color.value)
        ]

    def others_sections(self, player_index: int):
        oo = [
            p
            for p in self.players
            if p.ai_difficulty < 0 and p.index != player_index
        ]
        res = []
        for o in oo:
            pv = self.player_values(o) + [("Ip", self.tunnel)]
            res.append(self.kv_to_string(f"Other{o.index + 1}", pv))
        return "\n\n".join(res)

    def to_ini(self, player_index: int):
        player = next(p for p in self.players if p.index == player_index)
        ai_players = [p for p in self.players if p.ai_difficulty > -1]
        main_section_values = [
            ("Credits", self.start_credits),
            ("FrameSendRate", self.frame_send_rate),
            ("GameMode", 1),
            ("GameSpeed", self.game_speed),
            ("MCVRedeploy", self.mcv_redeploy),
            ("Protocol", self.protocol.value),
            ("Ra2Mode", self.ra2_mode),
            ("ShortGame", self.short_game),
            ("SidebarHack", "Yes"),
            ("SuperWeapons", self.superweapons),
            ("GameID", 12850),
            ("Bases", "Yes"),
            ("UnitCount", self.unit_count),
            ("UIGameMode", "Battle"),
            ("Host", player.is_host),
            ("Seed", self.seed),
            ("Scenario", "spawnmap.ini"),
            ("PlayerCount", len(self.players) - len(ai_players)),
            ("AIPlayers", len(ai_players)),
            ("Crates", self.crates),
            ("AlliesAllowed", self.allies_allowed),
            ("MultiEngineer", self.multi_engineer),
            ("BridgeDestory", self.bridges_destroyable),
            ("BuildOffAlly", self.build_off_ally),
        ]

        main_section_values.extend(self.player_values(player))

        res = []
        res.append(self.kv_to_string("Settings", main_section_values))
        res.append(self.others_sections(player_index))
        res.append(
            self.kv_to_string(
                "SpawnLocations",
                [(f"Multi{p.index + 1}", p.location) for p in self.players],
            )
        )
        res.append(
            self.kv_to_string(
                "Tunnel", [("Ip", self.tunnel), ("Port", self.port)]
            )
        )
        if ai_players:
            res.append(
                self.kv_to_string(
                    "HouseHandicaps",
                    [
                        (f"Multi{x.index + 1}", f"{x.ai_difficulty}")
                        for x in ai_players
                    ],
                )
            )
            res.append(
                self.kv_to_string(
                    "HouseCountries",
                    [(f"Multi{x.index + 1}", f"{x.side}") for x in ai_players],
                )
            )
            res.append(
                self.kv_to_string(
                    "HouseColors",
                    [
                        (f"Multi{x.index + 1}", f"{x.color.value}")
                        for x in ai_players
                    ],
                )
            )
        return "\n\n".join(res) + "\n"


def prun(args, **kwargs):
    cmdline = [str(x) for x in args]
    logging.info("exec: %s", cmdline)
    return subprocess.run(cmdline, **kwargs)


def popen(args, **kwargs):
    return subprocess.Popen([str(x) for x in args], **kwargs)


def docker_exit():
    subprocess.run(
        "docker-compose down --remove-orphans -t 1", shell=True, check=True
    )


def read_players_config(path) -> list[PlayerEntry]:
    D = json.loads(read_file(path))
    return [
        PlayerEntry(**{k: p.get(k, -1) for k in ["name", "ai_difficulty"]})
        for p in D["players"]
    ]


def try_fn(fn, retry_interval=2.0, tries=5):
    for _ in range(tries):
        try:
            r = fn()
            return r
        except KeyboardInterrupt:
            raise
        except:
            time.sleep(retry_interval)
    raise RuntimeError("Timeout")


class Docker:
    @classmethod
    def _common(cls, compose_files=None):
        cf = compose_files or ["docker-compose.yml"]
        r = ["docker-compose"]
        for c in cf:
            r.extend(["-f", c])
        return r

    @classmethod
    def run(
        cls,
        cmd,
        service,
        compose_files=None,
        uid=None,
        name=None,
        env=None,
    ):
        r = cls._common(compose_files=compose_files)
        r.extend(["run", "--rm", "-T"])
        if uid:
            r.extend(["-u", f"{uid}:{uid}"])
        if env:
            for k, v in env:
                r.extend(["-e", f"{k}={v}"])
        if name:
            r.append(f"--name={name}")
        r.append(service)
        r.extend(cmd)
        return r

    @classmethod
    def exec(cls, cmd, service, compose_files=None, uid=None, env=None):
        r = cls._common(compose_files=compose_files)
        r.extend(["exec", "-T"])
        if uid:
            r.extend(["-u", f"{uid}:{uid}"])
        if env:
            for k, v in env:
                r.extend(["-e", f"{k}={v}"])
        r.append(service)
        r.extend(cmd)
        return r

    @classmethod
    def up(cls, services, compose_files=None):
        r = cls._common(compose_files)
        r.append("up")
        r.extend(services)
        return r


def get_game_uid():
    return int(
        prun(
            Docker.run(
                [
                    "python3",
                    "-c",
                    'import os; print(os.stat("/home/user/project").st_uid)',
                ],
                "builder",
            ),
            check=True,
            capture_output=True,
        ).stdout.strip()
    )


class MainCommand:
    def __init__(self, args):
        self.args = args

    @functools.cached_property
    def uid(self):
        return get_game_uid()


def cmd_gamemd_patch():
    return [
        "python3",
        "./scripts/patch_gamemd.py",
        "-s",
        ".p_text:0x00004d66:0x00b7a000:0x0047e000",
        "-s",
        ".text:0x003df38d:0x00401000:0x00001000",
    ]


def cmd_addscn(addscn_path, dst):
    return [addscn_path, dst, ".p_text2", "0x1000", "0x60000020"]


class PatchGameMD(MainCommand):
    def main(self):
        args = self.args
        bdir = Path(args.build_dir)

        dst = args.output

        def _run(c):
            return prun(
                c,
                check=True,
                capture_output=True,
            )

        td = tempfile.TemporaryDirectory()

        s_temp = Path(td.name) / "tmp.exe"
        # Copy original spawner to temp directory
        shutil.copy(args.spawner_path, s_temp)

        # Append new section to copied spawner and write PE section info to text file
        o = _run(cmd_addscn(bdir / "addscn.exe", s_temp))
        pe_info_d = str(o.stdout, encoding="utf-8").strip()
        p_text_2_addr = pe_info_d.split(":")[2]

        # wait wine to exit
        if sys.platform != "win32":
            _run(["wineserver", "-w"])

        # Patch the binary
        o = _run(
            cmd_gamemd_patch()
            + [
                "-s",
                pe_info_d,
                "-d",
                p_text_2_addr,
                "-r",
                Path("data") / "patches.txt",
                "-i",
                f"{s_temp}",
                "-o",
                f"{dst}",
            ]
        )
        logging.info("patch successful: %s", dst.absolute())


class RunDockerInstance(MainCommand):
    def prepare_patched_spawner(self):
        args = self.args
        bdir = Path(args.build_dir)

        dst = bdir / NAME_SPAWNER_PATCHED
        if dst.exists():
            logging.info("not overwriting %s", str(dst))
            return

        dll_loader = bdir / "load_dll.bin"
        pe_info = bdir / "p_text2.txt"
        patch_info = bdir / "patches.txt"

        def _run(c):
            return prun(
                Docker.run(c, "builder", uid=self.uid),
                check=True,
                capture_output=True,
            )

        _run(
            [
                bdir / "ra2yrcppcli.exe",
                "--address-GetProcAddr=0x7e1250",
                "--address-LoadLibraryA=0x7e1220",
                f"--generate-dll-loader={dll_loader}",
            ]
        )

        # Copy original spawner to build directory
        shutil.copy(args.spawner_path, dst)

        # Append new section to copied spawner and write PE section info to text file
        o = _run([bdir / "addscn.exe", dst, ".p_text2", "0x1000", "0x60000020"])
        pe_info_d = str(o.stdout, encoding="utf-8").strip()
        p_text_2_addr = pe_info_d.split(":")[2]
        with open(pe_info, "w") as f:
            f.write(pe_info_d)

        # wait wine to eexit
        _run(["wineserver", "-w"])

        cmdline_patch = cmd_gamemd_patch() + [
            "-d",
            p_text_2_addr,
            "-s",
            pe_info_d,
            "-i",
            dst,
        ]

        # Get raw patch
        o = _run(
            cmdline_patch
            + [
                "-D",
                "-p",
                f"d0x7cd80f:{dll_loader}",
                "-o",
                patch_info,
            ]
        )

        # Patch the binary
        o = _run(
            cmdline_patch
            + [
                "-r",
                patch_info,
                "-o",
                f"{dst}",
            ]
        )

    def create_game_instance(
        self, i, player_name, port
    ) -> tuple[str, subprocess.Popen]:
        """Create Docker game instance

        Parameters
        ----------
        i : int
            Index of the player in the player list
        name : str
            Player name
        port : int
            ra2yrcpp server port

        Returns
        -------
        (str, subprocess.Popen)
            Tuple of container name, and docker process object.

        Raises
        ------
        subprocess.CalledProcessError: If the process fails.
        """
        args = self.args
        args_ini_overrides = list(
            sum([("-i", x) for x in args.ini_overrides], ())
        )
        cname = f"game-0-{i}"
        cmdline = Docker.run(
            ["./scripts/run-gamemd.py"]
            + args_ini_overrides
            + [
                "-p",
                args.players_config,
                "--base-dir",
                args.base_dir,
                "-B",
                args.build_dir,
                "-n",
                player_name,
                "-r",
                args.registry,
                "-t",
                args.type,
                "-g",
                args.game_mode,
                "-s",
                args.game_speed,
                "-S",
                args.spawner_path,
                "-Sy",
                args.syringe_spawner_path,
                "run-gamemd",
            ],
            "game-0",
            compose_files=[
                "docker-compose.yml",
                "docker-compose.integration.yml",
            ],
            uid=self.uid,
            name=cname,
            env=[
                ("RA2YRCPP_RECORD_PATH", str(int(time.time())) + ".pb.gz"),
                ("RA2YRCPP_PORT", f"{port}"),
            ],
        )
        return (cname, popen(cmdline))

    def run_docker_instance(self):
        pyra2yr_script = self.args.script.as_posix()
        os.environ["COMMAND_PYRA2YR"] = f"python3 {pyra2yr_script}"

        uid = self.uid
        main_service = popen(
            Docker.up("tunnel wm vnc novnc pyra2yr".split(" "))
        )

        # hack to wait until pyra2yr service has started
        try_fn(
            lambda: prun(
                Docker.exec(["ls", "-l"], "pyra2yr", uid=uid), check=True
            )
        )

        procs = []

        def _stop_instances():
            main_service.terminate()
            for cname, p in procs:
                prun(["docker", "stop", cname], check=True)
                p.wait()

        atexit.register(_stop_instances)

        # Create game instances
        for i, c in enumerate(
            p
            for p in ConfigFile.load(self.args.players_config).players
            if p.ai_difficulty < 0
        ):
            cname, proc = self.create_game_instance(i, c.name, 14520 + i + 1)
            procs.append((cname, proc))

        while True:
            time.sleep(1)


class RunGameMD:
    def __init__(self, args):
        self.args = args
        self.cfg = ConfigFile.load(self.args.players_config)
        self.players_config = self.cfg.players
        self.player_index, self.player_config = next(
            (i, p)
            for i, p in enumerate(self.players_config)
            if p.name == self.args.player_name
        )
        self.base_dir = Path(self.args.base_dir)
        self.instance_dir = self.base_dir / self.player_config.name
        self.build_dir = Path(self.args.build_dir)
        self.map_path = self.instance_dir / "spawnmap.ini"
        self.spawn_path = self.instance_dir / "spawn.ini"
        self.wineprefix_dir = self.instance_dir / ".wine"
        self.ra2yrcpp_spawner_path = self.instance_dir / NAME_SPAWNER_PATCHED

    def create_symlinks(self):
        """Symlink relevant data files for this test instance."""
        # Clear old symlinks
        for p in os.listdir(self.instance_dir):
            if os.path.islink(p):
                os.unlink(p)

        for p in re.split(r"\s+", FILE_PATHS):
            mklink(self.instance_dir / p, self.args.game_data_dir / p)

        for p in re.split(r"\s+", LIB_PATHS):
            mklink(self.instance_dir / p, (self.build_dir / p).absolute())

        for p in self.args.syringe_dlls:
            mklink(self.instance_dir / p, (self.build_dir / p).absolute())

        if self.args.syringe_spawner_path.exists():
            mklink(
                self.instance_dir / NAME_SPAWNER_SYRINGE,
                self.args.syringe_spawner_path.absolute(),
            )

    def create_ini_files(self):
        shutil.copy(self.cfg.map_path, self.map_path)

    def apply_ini_overrides(self):
        m = read_file(self.map_path)
        with open(self.map_path, "w") as f:
            f.write(
                "\n\n".join(
                    [m] + [read_file(p) for p in self.args.ini_overrides]
                )
            )

    def generate_spawnini(self):
        with open(self.spawn_path, "w") as f:
            f.write(self.cfg.to_ini(self.player_index))

    def prepare_wine_prefix(self):
        os.environ["WINEPREFIX"] = str(self.wineprefix_dir.absolute())
        if self.wineprefix_dir.exists():
            return
        self.wineprefix_dir.mkdir(parents=True)
        try:
            for c in (
                ["wineboot", "-ik"],
                ["wine", "regedit", self.args.registry],
                ["wineboot", "-s"],
                ["wineserver", "-w"],
            ):
                r = prun(c, check=True)
        except:
            logging.error("%s", traceback.format_exc())
            shutil.rmtree(self.wineprefix_dir)

    def prepare_instance_directory(self):
        self.create_symlinks()
        self.create_ini_files()
        self.apply_ini_overrides()
        self.generate_spawnini()

    def run(self):
        cmd = {
            "syringe": [
                "Syringe.exe",
                f" {NAME_SPAWNER_SYRINGE}",
                "-SPAWN",
                "-CD",
            ],
            "static": [NAME_SPAWNER_PATCHED, "-SPAWN"],
        }[self.args.type]
        so = open(self.instance_dir / "out.log", "w")
        se = open(self.instance_dir / "err.log", "w")
        prun(["wine"] + cmd, cwd=self.instance_dir, stdout=so, stderr=se)


def run_docker_instance(args):
    M = RunDockerInstance(args)
    atexit.register(docker_exit)
    try:
        if args.type == "static":
            M.prepare_patched_spawner()
        M.run_docker_instance()
    except KeyboardInterrupt:
        logging.info("Stop program")
    except subprocess.CalledProcessError as e:
        logging.error("%s %s", traceback.format_exc(), e.stderr)
    except Exception:
        logging.error("%s", traceback.format_exc())


def run_gamemd(args):
    M = RunGameMD(args)
    M.prepare_wine_prefix()
    M.prepare_instance_directory()
    M.run()


def patch_gamemd(args):
    M = PatchGameMD(args)
    try:
        M.main()
    except subprocess.CalledProcessError as e:
        logging.error("%s: %s", e.cmd, e.stderr)


def build(args):
    uid = get_game_uid()

    def _run(c):
        return prun(
            Docker.run("builder", c, uid=uid),
            check=True,
        )

    _run(["./scripts/tools.sh", "build-cpp"])


def parse_args():
    a = argparse.ArgumentParser(
        description="RA2YR game launcher helper",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    sp = a.add_subparsers(title="Subcommands", dest="subcommand")
    a1 = sp.add_parser(
        "run-docker-instance",
        help="Execute one or more game instances with Docker.",
    )
    a1.add_argument(
        "-e",
        "--script",
        help="pyra2yr script to run (relative to pyra2yr directory)",
        type=Path,
        default=Path("pyra2yr") / "test_sell_mcv.py",
    )
    a1.set_defaults(func=run_docker_instance)

    a2 = sp.add_parser("run-gamemd")
    a2.set_defaults(func=run_gamemd)

    a4 = sp.add_parser(
        "patch-gamemd", help="Patch original spawner for ra2yrcpp library."
    )
    a4.add_argument(
        "-o",
        "--output",
        help="Output for patched spawner",
        type=Path,
        default=Path(".") / NAME_SPAWNER_PATCHED,
    )
    a4.set_defaults(func=patch_gamemd)

    a3 = sp.add_parser("build")
    a3.add_argument(
        "-c",
        "--config",
        default="Release",
        choices=("Release", "RelWithDebInfo", "Debug"),
        help="CMake config type",
    )
    a3.add_argument(
        "-tc",
        "--toolchain",
        type=Path,
        default=Path("toolchains") / "mingw-w64-i686-docker.cmake",
        help="CMake toolchain file path",
    )
    a3.set_defaults(func=build)

    a.add_argument(
        "-b",
        "--base-dir",
        type=str,
        default=os.getenv("RA2YRCPP_TEST_INSTANCES_DIR"),
        help="Base directory for test instance directories.",
    )
    a.add_argument(
        "-t",
        "--type",
        type=str,
        choices=("static", "syringe"),
        default="static",
        help="The type of spawner to use. 'static' means statically patched spawner, 'syringe' uses unmodified spawner and loads provided DLL's using Syringe",
    )
    a.add_argument(
        "-T",
        "--tunnel-address",
        type=str,
        default="0.0.0.0",
        help="Tunnel server IP address.",
    )
    a.add_argument(
        "-tp",
        "--tunnel-port",
        type=int,
        default=50000,
        help="Tunnel server port.",
    )
    a.add_argument(
        "-d",
        "--syringe-dlls",
        type=str,
        action="append",
        default=[],
        help="DLL file (in the CnCNet main folder) to use for Syringe. This option can be specified multiple times.",
    )
    a.add_argument(
        "-p",
        "--players-config",
        type=str,
        default=os.path.join("test_data", "envs.json"),
        help="Path to Docker test instance configuration file.",
    )
    a.add_argument(
        "-i",
        "--ini-overrides",
        action="append",
        type=str,
        default=[],
        help="INI files to concatenate into the generated spawnmap ini.  This option can be specified multiple times.",
    )
    a.add_argument(
        "-n",
        "--player-name",
        type=str,
        help="Current player name.",
    )
    a.add_argument(
        "-r",
        "--registry",
        type=Path,
        default=Path("test_data") / "env.reg",
        help="Windows registry setup for the test environment.",
    )
    a.add_argument("-f", "--frame-send-rate", type=int, default=1)
    a.add_argument("-m", "--map-path", type=str, default=os.getenv("MAP_PATH"))
    a.add_argument(
        "-g",
        "--game-mode",
        type=str,
        choices=("ra2", "yr"),
        default="yr",
        help="Game mode",
    )
    a.add_argument(
        "-s",
        "--game-speed",
        type=int,
        choices=tuple(range(6)),
        default=0,
        help="Game speed. 0 = Fastest, 5 = Slowest.",
    )
    a.add_argument(
        "-G",
        "--game-data-dir",
        type=Path,
        default=os.getenv("RA2YRCPP_GAME_DIR"),
        help="Game data directory. You probably shouldn't modify this as it points to the default path specified in Docker compose and related files.",
    )
    a.add_argument(
        "-B",
        "--build-dir",
        type=Path,
        help="Path to folder containing ra2yrcpp library and related binaries.",
    )
    a.add_argument(
        "-S",
        "--spawner-path",
        required=True,
        type=Path,
        help="Path to original spawner binary.",
    )
    a.add_argument(
        "-Sy",
        "--syringe-spawner-path",
        type=Path,
        help="Path to syringe spawner (basically vanilla gamemd).",
    )
    return a.parse_args()


def main():
    setup_logging()
    a = parse_args()
    a.func(a)


if __name__ == "__main__":
    main()
