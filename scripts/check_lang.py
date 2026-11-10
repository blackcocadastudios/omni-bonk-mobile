#!/usr/bin/env python3
"""Checks the translation tables in src/lang/*.inc: same keys in every language, and each translation keeps
the printf specifiers (order included), the \\n line breaks and the trailing space of the English text."""
import re, sys, glob, os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'lang')
ENTRY = re.compile(r'^\{"((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)"\},\s*$')
SPEC = re.compile(r'%[-+0-9.]*[a-z%]')

def load(path):
    d = {}
    for n, line in enumerate(open(path, encoding='utf8'), 1):
        if line.startswith('//') or not line.strip():
            continue
        m = ENTRY.match(line)
        if not m:
            print(f'{path}:{n}: cannot parse entry'); sys.exit(1)
        if m.group(1) in d:
            print(f'{path}:{n}: duplicate key {m.group(1)!r}'); sys.exit(1)
        d[m.group(1)] = m.group(2)
    return d

files = sorted(glob.glob(os.path.join(ROOT, '*.inc')))
tables = {os.path.basename(f): load(f) for f in files}
ref_name = 'pt_BR.inc'
ref = tables[ref_name]
errors = 0
for name, t in tables.items():
    for k in ref.keys() - t.keys():
        print(f'{name}: missing key {k!r}'); errors += 1
    for k in t.keys() - ref.keys():
        print(f'{name}: extra key {k!r}'); errors += 1
    for k, v in t.items():
        if SPEC.findall(v) != SPEC.findall(k):
            print(f'{name}: printf specifiers differ for {k!r}: {SPEC.findall(v)} vs {SPEC.findall(k)}'); errors += 1
        if v.count('\\n') != k.count('\\n'):
            print(f'{name}: line breaks differ for {k!r}'); errors += 1
        if v.endswith(' ') != k.endswith(' '):
            print(f'{name}: trailing space differs for {k!r}'); errors += 1
print(f'{len(files)} languages, {len(ref)} entries each, {errors} problem(s)')
sys.exit(1 if errors else 0)
