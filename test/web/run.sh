#!/bin/bash
# Browser test of the web pages: serves webpage/ with fake camera data and clicks through every page in
# headless Chromium (Playwright). Checks the values shown, the switches, the Wi-Fi table, the tabs and that
# there are no JavaScript errors.
#
# Install once:
#   python3 -m venv ~/.venv-webtest
#   ~/.venv-webtest/bin/pip install playwright
#   ~/.venv-webtest/bin/python -m playwright install chromium-headless-shell
#
# Usage: PYTHON=~/.venv-webtest/bin/python test/web/run.sh

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
"${PYTHON:-python3}" -I "$HERE/test_pages.py" "$HERE/../../webpage"
