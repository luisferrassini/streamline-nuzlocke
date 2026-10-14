#!/usr/bin/env python3
"""Testes do gerador de header do caminho fixo.

Roda com: python3 tools/hack/test_gen_path_header.py
"""
import os
import re
import sys
import tempfile

# Permite importar o modulo do mesmo diretorio.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gen_path_header


JSON_SAMPLE = """\
{"version": 1, "rooms": [
    { "type": "route", "difficulty": 0, "name": "Route 103",
      "encounters": ["sunkern", "wingull", "ledyba"] },
    { "type": "route", "difficulty": 0, "name": "Route 102",
      "encounters": ["meowth", "sandshrew"] },
    { "type": "boss", "boss": "1", "encounters": [],
      "party": [ { "species": "geodude", "level": 15 },
                 { "species": "nosepass", "level": 14 } ] }
]}
"""


def _count_entry_lines(text):
    """Conta linhas que abrem uma entrada do array estatico."""
    return sum(1 for line in text.splitlines()
               if "ADVPATH_ROOM_" in line and "{" in line)


def _array_block(text, name):
    """Trecho do array estatico <name> (do marcador ate o '};' final)."""
    marker = name + "[]"
    if marker not in text:
        return ""
    start = text.index(marker)
    end = text.index("};", start)
    return text[start:end]


def main():
    failures = 0

    def check(name, ok):
        nonlocal failures
        if ok:
            print("PASS: %s" % name)
        else:
            print("FAIL: %s" % name)
            failures += 1

    with tempfile.TemporaryDirectory() as tmp:
        json_path = os.path.join(tmp, "hack_path.json")
        header_path = os.path.join(tmp, "hack_path.h")

        with open(json_path, "w") as f:
            f.write(JSON_SAMPLE)

        gen_path_header.generate(json_path, header_path)

        with open(header_path, "r") as f:
            text = f.read()

        lines = [line.strip() for line in text.splitlines() if line.strip()]
        first_entry = next(line for line in lines if line.startswith("{ ADVPATH_ROOM_"))
        entries = [line for line in lines if line.startswith("{ ADVPATH_ROOM_")]

        check("3 entradas no array", _count_entry_lines(text) == 3)
        check("primeira entrada e ROUTE", "ADVPATH_ROOM_ROUTE" in first_entry)
        check("ultima entrada e BOSS", "ADVPATH_ROOM_BOSS" in entries[-1])
        check("contador definido", "HACK_PATH_STEP_COUNT" in text)
        check("include do FixedPathStep", '#include "rogue_fixedpath.h"' in text)

        # Listas de encontros por sala (Task 1): ordem do JSON, uma por sala.
        check("array de encontros existe", "gHackPathEncounters[]" in text)
        check("contador de areas definido", "HACK_PATH_AREA_COUNT" in text)
        check("area0 tem sunkern", "SPECIES_SUNKERN" in text)
        check("area1 tem meowth", "SPECIES_MEOWTH" in text)
        check("chefe sem encontros", "{ NULL, 0 }," in text)
        # Uma linha de entrada do array por sala (3 salas). Isolado por bloco
        # para nao capturar as linhas vazias da tabela de times.
        enc_block = _array_block(text, "gHackPathEncounters")
        enc_entries = [line.strip() for line in enc_block.splitlines()
                       if re.match(r"^\{ (NULL|sHackPathEncounters[0-9]+), [0-9]+ \},$", line.strip())]
        check("3 entradas no array de encontros", len(enc_entries) == 3)
        check("area0 com 3 encontros", "{ sHackPathEncounters0, 3 }," in lines)
        # A ordem nao pode ser invertida: o chefe (ultima sala) tem 0 encontros.
        check("ultima entrada do array e vazia", enc_entries[-1] == "{ NULL, 0 },")

        # Nomes de area (Task 6): uma string por sala, na ordem do JSON.
        check("array de nomes existe", "gHackPathAreaNames[]" in text)
        check("contador de nomes definido", "HACK_PATH_NAME_COUNT" in text)
        check("nome da area0", '_("Route 103")' in text)
        check("nome da area1", '_("Route 102")' in text)
        check("nome vazio do chefe", '_("")' in text)

        # Times de chefe (Task 1): uma entrada por sala, na ordem do JSON.
        check("array de times existe", "gHackPathParties[]" in text)
        check("contador de times definido", "HACK_PATH_PARTY_COUNT" in text)
        party_block = _array_block(text, "gHackPathParties")
        party_entries = [line.strip() for line in party_block.splitlines()
                         if re.match(r"^\{ (NULL|sHackPathParty[0-9]+), [0-9]+ \},$", line.strip())]
        check("3 entradas no array de times", len(party_entries) == 3)
        check("mons do chefe no array estatico", "{ SPECIES_GEODUDE, 15 }" in text)
        check("mons do chefe no array estatico (2)", "{ SPECIES_NOSEPASS, 14 }" in text)
        check("sala de chefe referencia o array com count",
              "{ sHackPathParty2, 2 }," in lines)
        check("sala sem time fica vazia",
              party_entries[0] == "{ NULL, 0 }," if party_entries else False)

        # Tetos de nivel por sala de chefe, na ordem do JSON (Task 1).
        check("array de tetos existe", "gHackPathBossCaps[]" in text)
        check("contador de tetos definido", "HACK_PATH_BOSS_COUNT" in text)
        check("teto do primeiro chefe e 15", "gHackPathBossCaps[] = { 15 };" in text)

    # Os caminhos padrao (raiz do repo) nao podem cair dentro de tools/.
    repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    check("default json sob a raiz",
          os.path.abspath(gen_path_header.DEFAULT_JSON) == os.path.join(repo_root, "src", "data", "rogue", "hack_path.json"))
    check("default header sob a raiz",
          os.path.abspath(gen_path_header.DEFAULT_HEADER) == os.path.join(repo_root, "src", "data", "rogue", "hack_path.h"))

    # Um caminho maior que MAX_ROOMS deve falhar alto, nao cortar em silencio.
    limit_ok = {"version": 1,
                "rooms": [{"type": "route", "difficulty": 0}] * gen_path_header.MAX_ROOMS}
    try:
        gen_path_header.parse(limit_ok)
        check("aceita exatamente MAX_ROOMS", True)
    except ValueError:
        check("aceita exatamente MAX_ROOMS", False)

    too_big = {"version": 1,
               "rooms": [{"type": "route", "difficulty": 0}] * (gen_path_header.MAX_ROOMS + 1)}
    try:
        gen_path_header.parse(too_big)
        check("recusa caminho acima de MAX_ROOMS", False)
    except ValueError:
        check("recusa caminho acima de MAX_ROOMS", True)

    # Especie desconhecida deve falhar alto, nao gerar constante invalida.
    bad_species = {"version": 1,
                   "rooms": [{"type": "route", "difficulty": 0,
                              "encounters": ["nao-existe-xyz"]}]}
    try:
        gen_path_header.parse(bad_species)
        check("recusa especie desconhecida", False)
    except ValueError:
        check("recusa especie desconhecida", True)

    # Especie desconhecida ou nivel nao inteiro no time deve falhar alto.
    bad_party_species = {"version": 1,
                         "rooms": [{"type": "boss",
                                    "party": [{"species": "nao-existe-xyz", "level": 5}]}]}
    try:
        gen_path_header.parse(bad_party_species)
        check("recusa especie desconhecida no time", False)
    except ValueError:
        check("recusa especie desconhecida no time", True)

    bad_party_level = {"version": 1,
                       "rooms": [{"type": "boss",
                                  "party": [{"species": "geodude", "level": "abc"}]}]}
    try:
        gen_path_header.parse(bad_party_level)
        check("recusa nivel nao inteiro no time", False)
    except ValueError:
        check("recusa nivel nao inteiro no time", True)

    if failures:
        print("FALHAS: %d" % failures)
        return 1
    print("TUDO PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())