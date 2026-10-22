"""Validate actual WorldBridge reflection descriptors in a local 1.21.11 game jar.

Reads classfiles only; never launches or modifies Minecraft.
"""
from pathlib import Path
import argparse
import re
import tempfile
import zipfile
from minecraft_classfile import ClassArchive, remap_descriptor
from world_binding_fixture import write_native_bindings

ROOT = Path(__file__).resolve().parent.parent


def arguments(value):
    depth = 0
    start = 0
    result = []
    quoted = False
    for i, c in enumerate(value):
        if c == '"': quoted = not quoted
        if quoted: continue
        if c == "(": depth += 1
        elif c == ")": depth -= 1
        elif c == "," and depth == 0:
            result.append(value[start:i].strip()); start = i + 1
    result.append(value[start:].strip())
    return result


def calls(source):
    for match in re.finditer(r"mapped(Method|Field)\(", source):
        depth = 1
        end = match.end()
        quoted = False
        while depth:
            c = source[end]
            if c == '"': quoted = not quoted
            if not quoted:
                if c == "(": depth += 1
                if c == ")": depth -= 1
            end += 1
        yield match[1].lower(), arguments(source[match.end():end - 1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", required=True, type=Path)
    parser.add_argument("--intermediary", required=True, type=Path)
    args = parser.parse_args()
    with zipfile.ZipFile(args.intermediary) as jar:
        tiny = jar.read("mappings/mappings.tiny").decode().splitlines()
    if tiny[0] != "v1\tofficial\tintermediary":
        parser.error("This local 1.21.11 audit expects official/intermediary Tiny v1")
    classes = {p[1]: p[2] for line in tiny if (p := line.split("\t"))[0] == "CLASS"}
    inverse = {v: k for k, v in classes.items()}
    members = {}
    for line in tiny:
        parts = line.split("\t")
        if parts[0] not in ("FIELD", "METHOD"): continue
        kind, owner, descriptor, name, intermediary = parts
        key = (kind.lower(), intermediary, remap_descriptor(descriptor, classes))
        members.setdefault(key, []).append((owner, name, descriptor))
    text = (ROOT / "src/bridge/WorldBridge.java").read_text(encoding="utf-8")
    sections = []
    for start, end in (("private void prepareCrystal()", "private boolean crystalHeld"),
                       ("private void prepareAura()", "private boolean auraFlag"),
                       ("private void prepareAuraWeapons()", "private boolean auraWeaknessWeapon")):
        a = text.index(start); sections.append(text[a:text.index(end, a)])
    with tempfile.TemporaryDirectory(prefix="onestep-aura-bindings-") as work:
        pairs = write_native_bindings(Path(work) / "bindings.txt").read_text().splitlines()
        native = dict(zip(pairs[::2], pairs[1::2]))
    checks = 0
    with ClassArchive(args.client) as archive:
        for source in sections:
            aliases = {"clientType": "net/minecraft/class_310", "playerType": "net/minecraft/class_746",
                       "crystalHitType": "net/minecraft/class_3965"}
            aliases.update({m[1]: "net/minecraft/" + m[2] for m in re.finditer(r'(\w+)\s*=\s*load\("([^"]+)"\)', source)})
            def cls(value):
                match = re.fullmatch(r'load\("([^"]+)"\)', value)
                return "net/minecraft/" + match[1] if match else aliases[value]
            def desc(value):
                if value.endswith(".class"):
                    name = value[:-6]
                    primitives = {"int":"I", "float":"F", "double":"D", "long":"J", "boolean":"Z", "void":"V"}
                    standard={"String":"java/lang/String","Object":"java/lang/Object","List":"java/util/List"}
                    return primitives.get(name, "L" + standard.get(name,name.replace(".", "/")) + ";")
                return "L" + cls(value) + ";"
            expanded=[]
            for kind, values in calls(source):
                if values[1] == "slots[i]":
                    slots=re.search(r'String\[\] slots=\{([^}]+)\}',source)
                    assert slots, "Missing equipment slot binding list"
                    for symbol in re.findall(r'"([^"]+)"',slots[1]):
                        expanded.append((kind,[values[0],f'"{symbol}"',*values[2:]]))
                else:
                    expanded.append((kind,values))
            for kind, values in expanded:
                owner, symbol = cls(values[0]), values[1].strip('"')
                assert symbol in native, f"Missing native name for {symbol}"
                descriptor = desc(values[2]) if kind == "field" else "(" + "".join(desc(v) for v in values[3:]) + ")" + desc(values[2])
                actual_owner = inverse[owner]
                lineage = set(archive.lineage(actual_owner))
                records = members.get((kind, symbol, descriptor), [])
                found = None
                for real_owner, name, real_descriptor in records:
                    if real_owner not in lineage: continue
                    found = archive.find(actual_owner, kind, name, real_descriptor)
                    if found: break
                assert found, f"Missing real game binding: {owner}.{symbol}{descriptor}"
                if kind == "field":
                    expected = values[3] == "true"
                    assert bool(found[1] & 8) == expected, f"Static mismatch: {symbol}"
                else:
                    assert not found[1] & 8, f"Unexpected static method: {symbol}"
                checks += 1
    print(f"PASS: {checks} actual 1.21.11 field/method contracts for Crystal Aura + shared crystal I/O")


if __name__ == "__main__":
    main()
