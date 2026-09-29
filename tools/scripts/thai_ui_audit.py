"""Audit embedded UTF-8 HTML and identify English display candidates.

Candidates require human review: identifiers/protocol values are not UI copy.
"""
from pathlib import Path
from html.parser import HTMLParser
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

source = Path('src/web/WebAssets.h').read_bytes().decode('utf-8-sig')
assert '\ufffd' not in source, 'Replacement character in web assets'
assets = dict(re.findall(r'static const char k(\w+)\[\] PROGMEM = R"HTML\((.*?)\)HTML";', source, re.S))

class VisibleText(HTMLParser):
    def __init__(self):
        super().__init__()
        self.skip = 0
        self.text = []
    def handle_starttag(self, tag, attrs):
        if tag in ('script', 'style'):
            self.skip += 1
        for key, value in attrs:
            if key in ('placeholder', 'title', 'aria-label') and value:
                self.text.append(value)
    def handle_endtag(self, tag):
        if tag in ('script', 'style'):
            self.skip -= 1
    def handle_data(self, data):
        if not self.skip and data.strip():
            self.text.append(data.strip())

for name, html in assets.items():
    if '<!doctype' in html.lower():
        assert re.search(r'<html\s+lang="th"', html), f'{name}: lang is not Thai'
        assert re.search(r'<meta\s+charset="utf-8"', html, re.I), f'{name}: UTF-8 missing'
    parser = VisibleText()
    parser.feed(html)
    for text in parser.text:
        if re.search(r'[A-Za-z]{3}', text):
            print(f'{name} HTML candidate: {text}')
    for script in re.findall(r'<script>(.*?)</script>', html, re.S):
        for literal in re.findall(r"'([^'\n]*)'", script):
            if re.search(r'[A-Za-z]{3}\s+[A-Za-z]{3}', literal):
                print(f'{name} JS candidate: {literal}')
    print(f'{name}: UTF-8/metadata PASS')
