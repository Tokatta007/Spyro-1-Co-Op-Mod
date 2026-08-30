import argparse
import math
import os.path
import pathlib
import posixpath
import xml.etree.ElementTree as ET


class ArgNamespace:
    input_file: pathlib.Path
    output_file: pathlib.Path

parser = argparse.ArgumentParser()
parser.add_argument("input_file", type=pathlib.Path)
parser.add_argument("output_file", type=pathlib.Path)
args = parser.parse_args(namespace=ArgNamespace())

tree = ET.parse(args.input_file)
iso_project = tree.getroot()
track = iso_project.find("track")
directory_tree = track.find("directory_tree")

sp2_sectors = 23
sp2x2_sectors = 23


# Local change (macOS build): mkpsxiso 2.30's dumpsxiso no longer writes a
# "source" attribute on every <file>. The path is now implied by
# <directory_tree source="rom"> plus the enclosing <dir name="..."> elements.
# So walk the tree in document order and rebuild each path. Files that still
# carry an explicit "source" (older dumpsxiso output) are used as-is, so this
# works with both formats. The sector arithmetic below is unchanged.
def walk(parent, base):
    global sp2_sectors, sp2x2_sectors

    for child in parent:
        if child.tag == "file":
            sp2_source = child.attrib.get("source")
            if sp2_source is None:
                sp2_source = posixpath.join(base, child.attrib["name"])
            sp2x2_source = posixpath.join("build", sp2_source)
            sp2_size = os.path.getsize(sp2_source)
            sp2x2_size = os.path.getsize(sp2x2_source)
            sp2_sectors += math.ceil(sp2_size / 2048)
            sp2x2_sectors += math.ceil(sp2x2_size / 2048)

        elif child.tag == "dummy":
            sp2_dummy_sectors = int(child.attrib["sectors"])
            sp2x2_dummy_sectors = sp2_dummy_sectors + sp2_sectors - sp2x2_sectors
            child.attrib["sectors"] = str(sp2x2_dummy_sectors)
            print(child.attrib["sectors"])
            sp2_sectors += sp2_dummy_sectors
            sp2x2_sectors += sp2x2_dummy_sectors

        elif child.tag == "dir":
            walk(child, posixpath.join(base, child.attrib["name"]))


walk(directory_tree, directory_tree.attrib.get("source", ""))

tree.write(args.output_file)
