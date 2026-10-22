"""Read-only audit of Inventory Walk action bindings against a real 1.21.11 jar."""
from pathlib import Path
import argparse
import re
import zipfile
from minecraft_classfile import ClassArchive, remap_descriptor

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", required=True, type=Path)
    parser.add_argument("--intermediary", required=True, type=Path)
    args = parser.parse_args()
    source = (ROOT / "src/bridge/ClientBridge.java").read_text(encoding="utf-8")
    section = source.split("private void prepareWalkActions()", 1)[1].split("/** Runs before", 1)[0]
    # Read the actual source expressions, so changing a binding also changes this audit.
    aliases = {"playerType": "net/minecraft/class_746"}
    aliases.update({name: "net/minecraft/" + symbol for name, symbol in
                    re.findall(r'(\w+)\s*=\s*load\("([^"]+)"\)', section)})
    field_pattern = r'field\(\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*(\w+)\s*,\s*(true|false)\s*\)'
    contracts = []
    for owner, name, kind, stat in re.findall(field_pattern, section):
        contracts.append(("field", aliases[owner], name, "L" + aliases[kind] + ";", stat == "true"))
    timer = re.search(r'sprintTimer\s*=\s*field\(playerType,\s*"([^"]+)",\s*int.class,\s*false\)', source)
    assert timer, "Missing exact sprint timer binding"
    contracts.append(("field", aliases["playerType"], timer[1], "I", False))
    method_pattern = r'method\(\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*void.class\s*(?:,\s*(\w+)\s*)?\)'
    for owner, name, parameter in re.findall(method_pattern, section):
        descriptor = "(" + ("L" + aliases[parameter] + ";" if parameter else "") + ")V"
        contracts.append(("method", aliases[owner], name, descriptor, False))
    constructor = re.search(r'load\("([^"]+)"\).getDeclaredConstructor\((\w+)\)', section)
    assert constructor, "Missing input packet constructor"
    contracts.append(("method", "net/minecraft/" + constructor[1], "<init>",
                      "(L" + aliases[constructor[2]] + ";)V", False))
    assert len(contracts) == 11, f"Incomplete action audit: {len(contracts)} contracts"
    with zipfile.ZipFile(args.intermediary) as jar:
        tiny = jar.read("mappings/mappings.tiny").decode().splitlines()
    assert tiny[0] == "v1\tofficial\tintermediary"
    classes = {p[1]: p[2] for line in tiny if (p := line.split("\t"))[0] == "CLASS"}
    inverse = {value: key for key, value in classes.items()}
    records = []
    for line in tiny:
        parts = line.split("\t")
        if parts[0] in ("FIELD", "METHOD"):
            kind, owner, descriptor, name, intermediary = parts
            records.append((kind.lower(), owner, descriptor, name, intermediary))
    with ClassArchive(args.client) as archive:
        for kind, owner, name, descriptor, static in contracts:
            actual_owner = inverse[owner]
            actual_descriptor = remap_descriptor(descriptor, inverse)
            if name == "<init>":
                found = archive.find(actual_owner, kind, name, actual_descriptor)
            else:
                lineage = set(archive.lineage(actual_owner))
                matches = [(real_owner, real_name) for real_kind, real_owner, real_desc, real_name, symbol in records
                           if real_kind == kind and symbol == name and real_desc == actual_descriptor and real_owner in lineage]
                found = next((result for _, real_name in matches
                              if (result := archive.find(actual_owner, kind, real_name, actual_descriptor))), None)
            assert found, f"Missing actual game binding: {owner}.{name}{descriptor}"
            assert bool(found[1] & 8) == static, f"Static mismatch: {name}"
    print(f"PASS: {len(contracts)} actual 1.21.11 Inventory Walk action contracts")


if __name__ == "__main__":
    main()
