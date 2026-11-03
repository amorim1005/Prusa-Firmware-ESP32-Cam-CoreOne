#!/usr/bin/env python3
"""Generate ESP32_PrusaConnectCam/WebPage.h from the files in webpage/.

- Every web file is copied verbatim into its R"rawliteral(...)rawliteral" block of WebPage.h
  (backslashes and line endings are kept as they are, unlike the old awk script).
- The ?v= cache version in the webpage/*.html files is set to SW_VERSION from mcu_cfg.h,
  so the browser loads the new styles.css and scripts.js after a firmware update.

Usage:
  python3 webpage/webpage_h_generator.py           update webpage/*.html and WebPage.h
  python3 webpage/webpage_h_generator.py --check   only check, exit 1 when something is out of date (CI)
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SKETCH = os.path.join(HERE, '..', 'ESP32_PrusaConnectCam')
WEBPAGE_H = os.path.join(SKETCH, 'WebPage.h')
MCU_CFG_H = os.path.join(SKETCH, 'mcu_cfg.h')

# web file -> C symbol in WebPage.h
FILES = {
    'index.html': 'index_html',
    'page_auth.html': 'page_auth_html',
    'page_wifi.html': 'page_wifi_html',
    'page_config.html': 'page_config_html',
    'page_system.html': 'page_system_html',
    'styles.css': 'styles_css',
    'scripts.js': 'scripts_js',
    'license.html': 'license_html',
    'stream.html': 'stream_html',
}


def read(path):
    with open(path, newline='') as f:
        return f.read()


def write(path, text):
    with open(path, 'w', newline='') as f:
        f.write(text)


def sw_version():
    m = re.search(r'#define\s+SW_VERSION\s+"([^"]+)"', read(MCU_CFG_H))
    if not m:
        sys.exit('SW_VERSION not found in mcu_cfg.h')
    return m.group(1)


def main():
    check = '--check' in sys.argv[1:]
    version = sw_version()
    outdated = []

    # ?v= cache version in the HTML files
    sources = {}
    for name in FILES:
        path = os.path.join(HERE, name)
        text = read(path)
        if name.endswith('.html'):
            new = re.sub(r'\?v=[0-9A-Za-z.\-]+', '?v=' + version, text)
            if new != text:
                outdated.append('%s: ?v= is not %s' % (name, version))
                text = new
                if not check:
                    write(path, text)
        sources[name] = text

    # WebPage.h blocks: the content ends with a newline, like the old generator
    header = read(WEBPAGE_H)
    new_header = header
    for name, symbol in FILES.items():
        content = sources[name]
        if not content.endswith('\n'):
            content += '\n'
        if ')rawliteral"' in content:
            sys.exit('%s contains )rawliteral", it would end the C string' % name)
        pattern = re.compile(r'(const char ' + symbol + r'\[\] PROGMEM = R"rawliteral\(\n).*?(\)rawliteral";)', re.S)
        if not pattern.search(new_header):
            sys.exit('block %s not found in WebPage.h' % symbol)
        new_header = pattern.sub(lambda m: m.group(1) + content + m.group(2), new_header, count=1)

    if new_header != header:
        outdated.append('WebPage.h is not generated from webpage/')
        if not check:
            write(WEBPAGE_H, new_header)

    if check:
        for line in outdated:
            print('OUT OF DATE: ' + line)
        sys.exit(1 if outdated else 0)
    print('WebPage.h generated, version %s%s' % (version, ' (updated: %s)' % '; '.join(outdated) if outdated else ', nothing changed'))


if __name__ == '__main__':
    main()
