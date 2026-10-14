#!/usr/bin/env python3
"""Gera o header do caminho fixo a partir de src/data/rogue/hack_path.json.

Uso:
    python3 tools/hack/gen_path_header.py [caminho_do_json] [caminho_do_header]
"""
import json
import os
import re
import sys

# type do JSON -> constante ADVPATH_ROOM_*
ROOM_TYPES = {
    "route": "ADVPATH_ROOM_ROUTE",
    "boss": "ADVPATH_ROOM_BOSS",
}

# Precisa bater com ROGUE_ADVPATH_ROOM_CAPACITY em include/constants/rogue.h.
# Um caminho maior que isto perderia a sala final (o chefe) no jogo.
MAX_ROOMS = 32

_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_JSON = os.path.join(_ROOT, "src", "data", "rogue", "hack_path.json")
DEFAULT_HEADER = os.path.join(_ROOT, "src", "data", "rogue", "hack_path.h")
DEFAULT_SPECIES_H = os.path.join(_ROOT, "include", "constants", "species.h")


def species_const(name):
    """Nome do Kaizo (ex.: 'nidoran-m') -> constante C (ex.: SPECIES_NIDORAN_M)."""
    key = name.upper()
    for a, b in (("-", "_"), (" ", "_"), (".", ""), ("'", "")):
        key = key.replace(a, b)
    key = re.sub(r"[^A-Z0-9_]", "", key)
    return "SPECIES_" + key


def c_escape(text):
    """Escapa texto para virar literal C entre aspas duplas."""
    return text.replace("\\", "\\\\").replace('"', '\\"')


def load_species_constants(path=None):
    """Conjunto das constantes SPECIES_* declaradas em species.h."""
    if path is None:
        path = DEFAULT_SPECIES_H
    found = set()
    with open(path, "r") as f:
        for line in f:
            m = re.match(r"#define (SPECIES_[A-Z0-9_]+)\b", line)
            if m:
                found.add(m.group(1))
    if not found:
        raise ValueError("nenhuma constante SPECIES_ encontrada em %s" % path)
    return found


def parse(data):
    """Converte o JSON em (entradas, listas_de_encontros, nomes, listas_de_times).

    entradas: [(roomType, param, extra)]
    listas_de_encontros: [[SPECIES_*, ...]] na ordem do JSON (nao invertida).
    nomes: [str] na ordem do JSON (nome da area; "" se faltar).
    listas_de_times: [[(SPECIES_*, level), ...]] na ordem do JSON ([] se faltar).
    """
    if data.get("version") != 1:
        raise ValueError("versao do json desconhecida: %r" % data.get("version"))
    species = load_species_constants()
    entries = []
    encounter_lists = []
    names = []
    party_lists = []
    for room in data.get("rooms", []):
        rtype = room.get("type")
        if rtype not in ROOM_TYPES:
            raise ValueError("tipo de sala desconhecido: %r" % rtype)
        if rtype == "boss":
            entries.append((ROOM_TYPES[rtype], 0, int(room.get("trainer", 0))))
        else:
            entries.append((ROOM_TYPES[rtype], int(room.get("difficulty", 0)), 0))

        names.append(room.get("name", ""))

        names_list = room.get("encounters") or []
        consts = []
        for name in names_list:
            const = species_const(name)
            if const not in species:
                raise ValueError("especie desconhecida: %r (%s)" % (name, const))
            consts.append(const)
        encounter_lists.append(consts)

        # Time de chefe: validar cada especie e cada nivel (falhar alto).
        party = room.get("party") or []
        mons = []
        for mon in party:
            const = species_const(mon.get("species", ""))
            if const not in species:
                raise ValueError("especie desconhecida no time: %r (%s)"
                                 % (mon.get("species"), const))
            try:
                level = int(mon.get("level"))
            except (TypeError, ValueError):
                raise ValueError("nivel invalido no time: %r" % mon.get("level"))
            mons.append((const, level))
        party_lists.append(mons)

    if len(entries) > MAX_ROOMS:
        raise ValueError("caminho com %d salas; o limite e %d (ROGUE_ADVPATH_ROOM_CAPACITY)"
                         % (len(entries), MAX_ROOMS))
    return entries, encounter_lists, names, party_lists


def render(entries, encounter_lists, names, party_lists):
    """Monta o texto do header C a partir das entradas e listas de encontros."""
    lines = [
        "// AUTO-GERADO por tools/hack/gen_path_header.py. NAO edite a mao.",
        "#ifndef GUARD_DATA_ROGUE_HACK_PATH_H",
        "#define GUARD_DATA_ROGUE_HACK_PATH_H",
        "",
        '#include "rogue_fixedpath.h"',
        "",
        "static const struct FixedPathStep gHackPathSteps[] =",
        "{",
    ]
    for room_type, param, extra in entries:
        lines.append("    { %s, %d, %d }," % (room_type, param, extra))
    lines += [
        "};",
        "",
    ]

    # Arrays de especies por area e a tabela que os reune, na ordem do JSON.
    table = []
    for i, consts in enumerate(encounter_lists):
        if consts:
            lines.append("static const u16 sHackPathEncounters%d[] = { %s };"
                         % (i, ", ".join(consts)))
            table.append("{ sHackPathEncounters%d, %d }," % (i, len(consts)))
        else:
            table.append("{ NULL, 0 },")
    lines += [
        "static const struct FixedPathRoomEncounters gHackPathEncounters[] =",
        "{",
    ]
    lines += ["    %s" % row for row in table]
    lines += [
        "};",
        "",
    ]

    # Times de chefe por sala e a tabela que os reune, na ordem do JSON.
    party_table = []
    for i, mons in enumerate(party_lists):
        if mons:
            body = ", ".join("{ %s, %d }" % (const, level) for const, level in mons)
            lines.append("static const struct FixedPathPartyMon sHackPathParty%d[] = { %s };"
                         % (i, body))
            party_table.append("{ sHackPathParty%d, %d }," % (i, len(mons)))
        else:
            party_table.append("{ NULL, 0 },")
    lines += [
        "static const struct FixedPathRoomParty gHackPathParties[] =",
        "{",
    ]
    lines += ["    %s" % row for row in party_table]
    lines += [
        "};",
        "",
    ]

    # Teto de nivel: maior nivel de cada sala de chefe, na ordem do JSON.
    caps = []
    for i, (room_type, _, _) in enumerate(entries):
        if room_type == ROOM_TYPES["boss"]:
            mons = party_lists[i]
            caps.append(max(level for _, level in mons) if mons else 0)
    lines.append("static const u8 gHackPathBossCaps[] = { %s };"
                 % ", ".join(str(c) for c in caps))
    lines += [
        "",
    ]

    # Nomes de area, na ordem do JSON (para o menu de ofertas pendentes).
    for i, name in enumerate(names):
        lines.append('static const u8 sHackPathName%d[] = _("%s");' % (i, c_escape(name)))
    lines += [
        "static const u8 *const gHackPathAreaNames[] =",
        "{",
    ]
    lines += ["    sHackPathName%d," % i for i in range(len(names))]
    lines += [
        "};",
        "",
        "#define HACK_PATH_STEP_COUNT ARRAY_COUNT(gHackPathSteps)",
        "#define HACK_PATH_AREA_COUNT ARRAY_COUNT(gHackPathEncounters)",
        "#define HACK_PATH_NAME_COUNT ARRAY_COUNT(gHackPathAreaNames)",
        "#define HACK_PATH_PARTY_COUNT ARRAY_COUNT(gHackPathParties)",
        "#define HACK_PATH_BOSS_COUNT ARRAY_COUNT(gHackPathBossCaps)",
        "",
        "#endif // GUARD_DATA_ROGUE_HACK_PATH_H",
    ]
    return "\n".join(lines)


def generate(json_path, header_path):
    """Le o JSON e escreve o header."""
    with open(json_path, "r") as f:
        data = json.load(f)
    entries, encounter_lists, names, party_lists = parse(data)
    header = render(entries, encounter_lists, names, party_lists)
    header_dir = os.path.dirname(header_path)
    if header_dir:
        os.makedirs(header_dir, exist_ok=True)
    with open(header_path, "w") as f:
        f.write(header)
        f.write("\n")


def main(argv):
    if len(argv) > 2:
        print("uso: gen_path_header.py [json] [header]", file=sys.stderr)
        return 2
    json_path = argv[0] if len(argv) >= 1 else DEFAULT_JSON
    header_path = argv[1] if len(argv) >= 2 else DEFAULT_HEADER
    generate(json_path, header_path)
    print("gerado: %s" % header_path)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))