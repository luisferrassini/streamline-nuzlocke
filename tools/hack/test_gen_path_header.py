#!/usr/bin/env python3
"""Testes do gerador de header do caminho fixo.

Roda com: python3 tools/hack/test_gen_path_header.py
"""
import os
import sys
import tempfile

# Permite importar o modulo do mesmo diretorio.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gen_path_header


JSON_SAMPLE = """\
{"version": 1, "rooms": [
    { "type": "route", "difficulty": 0 },
    { "type": "route", "difficulty": 0 },
    { "type": "boss",  "trainer": 0 }
]}
"""


def _count_entry_lines(text):
    """Conta linhas que abrem uma entrada do array estatico."""
    return sum(1 for line in text.splitlines()
               if "ADVPATH_ROOM_" in line and "{" in line)


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

    # Os caminhos padrao (raiz do repo) nao podem cair dentro de tools/.
    repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    check("default json sob a raiz",
          os.path.abspath(gen_path_header.DEFAULT_JSON) == os.path.join(repo_root, "src", "data", "rogue", "hack_path.json"))
    check("default header sob a raiz",
          os.path.abspath(gen_path_header.DEFAULT_HEADER) == os.path.join(repo_root, "src", "data", "rogue", "hack_path.h"))

    if failures:
        print("FALHAS: %d" % failures)
        return 1
    print("TUDO PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())