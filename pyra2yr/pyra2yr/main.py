import argparse
import gzip

from google.protobuf.json_format import MessageToJson

from pyra2yr.util import read_protobuf_messages


def parse_args():
    a = argparse.ArgumentParser(
        description="pyra2yr main utility",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    a.add_argument("-d", "--dump-replay", action="store_true")
    a.add_argument(
        "-i", "--input-path", help="input path if applicable", type=str
    )
    return a.parse_args()


def dump_replay(path: str):
    with gzip.open(path, "rb") as f:
        m = read_protobuf_messages(f)
        for _, m0 in enumerate(m):
            print(MessageToJson(m0))


def main():
    # pylint: disable=unused-variable
    args = parse_args()
    if args.dump_replay:
        dump_replay(args.input_path)


if __name__ == "__main__":
    main()
