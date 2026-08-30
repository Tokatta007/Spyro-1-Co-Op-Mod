import argparse
import pathlib


class ArgNamespace:
    directory: pathlib.Path
    parents: bool

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=pathlib.Path)
parser.add_argument("-p", "--parents", action="store_true")
args = parser.parse_args(namespace=ArgNamespace())

args.directory.mkdir(parents=args.parents, exist_ok=args.parents)
