import argparse
import pathlib
import shutil


class ArgNamespace:
    source: pathlib.Path
    dest: pathlib.Path

parser = argparse.ArgumentParser()
parser.add_argument("source", type=pathlib.Path)
parser.add_argument("dest", type=pathlib.Path)
args = parser.parse_args(namespace=ArgNamespace())

shutil.copy(args.source, args.dest)
