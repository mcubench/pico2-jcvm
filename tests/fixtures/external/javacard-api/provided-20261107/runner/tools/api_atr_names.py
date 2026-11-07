#!/usr/bin/env python3
"""Downloads https://www.fi.muni.cz/~xsvenda/jcalgtest/table.html and stores the mapping ATR -> card name.

    python3 api_atr_names.py                       # -> docs/card_results/atr_names.csv (ATR;name)
    python3 api_atr_names.py --url file.html --out names.csv

api_results_html.py uses that file to show human-readable card names (matched by ATR, blanks/case ignored).
Cards missing in the public table are added by hand to atr_names_local.csv (same format, never overwritten by this script;
its entries beat the downloaded ones).
"""
import argparse
import csv
import html
import os
import re
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
URL = "https://www.fi.muni.cz/~xsvenda/jcalgtest/table.html"
ROW = re.compile(r'<b>c\d+</b>\s*<a [^>]*>([^<]*)</a>\s*,\s*ATR=([0-9A-Fa-f ]+?)\s*[\(,<]')


def norm_atr(s):
    return re.sub(r"\s", "", s).upper()


def parse(text):
    out = {}
    for n, a in ROW.findall(text):  # one ATR can belong to several cards: keep all names
        names = out.setdefault(norm_atr(a), [])
        n = html.unescape(n).strip()
        if n not in names:
            names.append(n)
    return {a: " / ".join(n) for a, n in out.items()}


def load(path):
    """ATR -> name from a file written by this script; {} when the file is missing."""
    if not os.path.exists(path):
        return {}
    with open(path, encoding="utf-8", newline="") as f:
        return {norm_atr(r["atr"]): r["name"] for r in csv.DictReader(f, delimiter=";")}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--url", default=URL, help="url or local html file")
    ap.add_argument("--out", default=os.path.join(HERE, "..", "docs", "card_results", "atr_names.csv"))
    a = ap.parse_args()
    if os.path.exists(a.url):
        text = open(a.url, encoding="utf-8", errors="replace").read()
    else:
        text = urllib.request.urlopen(a.url, timeout=60).read().decode("utf-8", errors="replace")
    m = parse(text)
    if not m:
        raise SystemExit("no 'cN <a>name</a>, ATR=...' entries found in " + a.url)
    with open(a.out, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter=";", lineterminator="\n")
        w.writerow(["atr", "name"])
        for atr, name in sorted(m.items(), key=lambda x: x[1].lower()):
            w.writerow([atr, name])
    print("%d cards -> %s" % (len(m), a.out))


if __name__ == "__main__":
    main()
