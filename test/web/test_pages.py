"""Browser test of the web pages: serves webpage/ with fake camera data and clicks through them in headless Chromium."""
import http.server
import json
import os
import sys
import threading
import urllib.parse

from playwright.sync_api import sync_playwright

WEB = sys.argv[1]
requests_seen = []

STATUS = {
    "mdns": "prusa-esp32cam", "upload_interval": "10", "fingerprint": "FPRINT123", "token": "abcdefghijklmnopqrst",
    "refreshInterval": 10, "photoquality": 12, "framesize": 6, "framesize_supported": "3,4,5,6",
    "camera_detected": "OV5640", "camera_autofocus": "Continuous", "brightness": 0, "contrast": 0, "saturation": 0,
    "image_rotation": 1, "hmirror": "false", "vflip": "true", "lensc": "true", "exposure_ctrl": "true",
    "awb": "true", "awb_gain": "true", "wb_mode": 0, "bpc": "false", "wpc": "true", "raw_gama": "true",
    "aec2": "false", "ae_level": 0, "aec_value": 300, "gain_ctrl": "true", "agc_gain": 0, "timelaps": "false",
    "prusalink_enable": "true", "prusalink_host": "192.168.1.50", "prusalink_user": "maker",
    "prusalink_status": "OK", "printer_state": "PRINTING", "job_progress": "42",
    "auth": "false", "auth_username": "admin", "serviceap": "true", "ssid": "HomeNet", "rssi": -55,
    "rssi_percentage": 80, "ip": "192.168.1.60", "wifi_network_status": "Connected", "ip_cfg": 0,
    "net_ip": "192.168.0.10", "net_mask": "255.255.255.0", "net_gw": "192.168.0.1", "net_dns": "8.8.8.8",
    "uptime": "1 h", "sw_ver": "1.1.2-c10", "sw_build": "today", "last_upload_status": "Photo: 204 - Upload OK",
    "wifi_mode": "STA", "service_ap_ssid": "ESP32_camera_X", "sd_status": "Card detected", "sd_total": 1000,
    "sd_free_p": 70, "sd_used_p": 30, "mcu_temp": "55 *C", "health_status": "OK", "upload_ok": 12,
    "upload_failed": 1, "unplanned_restarts": 0, "last_restart": "Power on", "mcu_temp_max": "60 *C",
    "mem_internal": "100 KB", "mem_psram": "7000 KB", "log_level": 2,
}
WIFI = [{"ssid": "HomeNet", "rssi": -55, "rssi_percentage": 80, "channel": 6, "encryption": "WPA2"},
        {"ssid": "<b>evil</b>", "rssi": -80, "rssi_percentage": 20, "channel": 11, "encryption": "OPEN"}]


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, body, ctype='text/plain'):
        data = body.encode() if isinstance(body, str) else body
        self.send_response(200)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = urllib.parse.urlparse(self.path).path.lstrip('/') or 'index.html'
        requests_seen.append(self.path)
        if path == 'json_input':
            return self.send(json.dumps(STATUS))
        if path == 'json_wifi':
            return self.send(json.dumps(WIFI), 'application/json')
        for d in (WEB, os.path.join(WEB, 'assets/icons'), os.path.join(WEB, 'assets/logo')):
            f = os.path.join(d, path)
            if os.path.isfile(f):
                ctype = {'html': 'text/html', 'js': 'application/javascript', 'css': 'text/css', 'svg': 'image/svg+xml'}.get(path.rsplit('.', 1)[-1], 'application/octet-stream')
                return self.send(open(f, 'rb').read(), ctype)
        if path.endswith('.jpg'):
            return self.send(b'', 'image/jpeg')
        return self.send('OK')   # set_* routes


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
base = 'http://127.0.0.1:%d/' % server.server_port

failures = []
def check(cond, what):
    print(('  PASS ' if cond else '  FAIL ') + what)
    if not cond:
        failures.append(what)

with sync_playwright() as p:
    browser = p.chromium.launch()
    page = browser.new_page()
    errors = []
    page.on('pageerror', lambda e: errors.append(str(e)))
    page.on('console', lambda m: errors.append(m.text) if m.type == 'error' else None)
    page.on('dialog', lambda d: d.accept())

    print('index + Camera page')
    page.goto(base)
    page.wait_for_selector('#tokenid')
    page.wait_for_function("document.getElementById('tokenid').value != ''")
    check(page.title() == 'prusa-esp32cam', 'title from json_input')
    check(page.inner_text('#refreshInterval') == '10', 'upload interval on the main page')
    check(page.input_value('#tokenid') == 'abcdefghijklmnopqrst', 'token field filled')
    check(page.inner_text('#fingerprint') == 'FPRINT123', 'fingerprint text')
    check(page.inner_text('#camera_detected') == 'OV5640', 'detected sensor text')
    check(page.inner_text('#printer_state') == 'PRINTING' and page.inner_text('#job_progress') == '42 %', 'PrusaLink state and progress')
    check(page.inner_text('#status_vflip') == 'On', 'switch status text')
    check(page.is_checked('#prusalinkid'), 'PrusaLink switch checked')
    check(page.is_checked('#vflipid') and not page.is_checked('#hmirrorid') and not page.is_checked('#bpcid'), 'switches follow "true"/"false" (bug of the original page)')
    hidden = page.eval_on_selector('#framesizeid', "s => Array.from(s.options).filter(o => o.hidden).length")
    check(hidden > 0, 'unsupported resolutions hidden')
    check(page.query_selector('#ledid') is None and page.query_selector('#flashid') is None, 'no LED / flash controls')
    check(page.query_selector('#light-icon') is None, 'no light button')
    check('active' in (page.get_attribute('a[href="page_config.html"]', 'class') or ''), 'Camera tab active')

    print('collapsible section')
    content = page.locator('.btn_collapsible').first
    before = page.eval_on_selector('.btn_collapsible', "b => getComputedStyle(b.parentElement.nextElementSibling).display")
    content.click()
    after = page.eval_on_selector('.btn_collapsible', "b => getComputedStyle(b.parentElement.nextElementSibling).display")
    check(before == 'none' and after == 'block', 'advanced settings open (%s -> %s)' % (before, after))
    content.click()
    again = page.eval_on_selector('.btn_collapsible', "b => getComputedStyle(b.parentElement.nextElementSibling).display")
    check(again == 'none', 'advanced settings close')

    print('setting a value')
    requests_seen.clear()
    page.click('#hmirrorid + .checkbox_slider')
    page.wait_for_timeout(300)
    check(any(r.startswith('/set_bool?hmirror=true') for r in requests_seen), 'switch sends set_bool?hmirror=true')

    print('Wi-Fi page')
    page.click('a[href="page_wifi.html"]')
    page.wait_for_selector('#wifi_ntw')
    page.wait_for_function("document.getElementById('wifi_ntw').rows.length == 3", timeout=5000)
    rows = page.eval_on_selector('#wifi_ntw', "t => Array.from(t.rows).slice(1).map(r => Array.from(r.cells).map(c => c.textContent))")
    check(rows[0][0] == 'HomeNet' and '80% / -55dBm' in rows[0][1] and rows[0][2] == '6', 'Wi-Fi table row: %s' % rows[0])
    check(rows[1][0] == '<b>evil</b>', 'SSID shown as text, not HTML')
    page.wait_for_function("document.getElementById('ssid') && document.getElementById('ssid').textContent == 'HomeNet'")
    check(page.inner_text('#ip') == '192.168.1.60', 'Wi-Fi status text')
    check('active' in (page.get_attribute('a[href="page_wifi.html"]', 'class') or ''), 'Wi-Fi tab active')

    print('Authentication page')
    page.click('a[href="page_auth.html"]')
    page.wait_for_function("document.getElementById('auth_username') && document.getElementById('auth_username').value == 'admin'")
    check(True, 'auth page filled')

    print('System page')
    page.click('a[href="page_system.html"]')
    page.wait_for_function("document.getElementById('upload_ok') && document.getElementById('upload_ok').textContent == '12'")
    check(page.inner_text('#health_status') == 'OK', 'health status')
    check(page.inner_text('#sw_ver') == '1.1.2-c10', 'version')
    check(page.eval_on_selector('#progress_bar_sd_used', 'e => e.style.width') == '30%', 'SD progress bar')

    print('licence page (footer)')
    page.click('a.page-link')
    page.wait_for_function("document.getElementById('content').textContent.indexOf('GNU') >= 0 || document.getElementById('content').textContent.indexOf('licen') >= 0")
    check(True, 'licence loaded in the content')
    check(page.query_selector('a[href="gtac.html"]') is None, 'no Prusa legal links')
    gh = page.get_attribute('#github-link', 'href')
    check('amorim1005' in gh, 'GitHub link to the fork')

    print('no jQuery requested, no JavaScript errors')
    check(not any('jquery' in r for r in requests_seen), 'jquery never requested')
    check(not errors, 'no errors: %s' % errors)
    browser.close()

print('\n%s: %d failure(s)' % ('FAILED' if failures else 'ALL PASSED', len(failures)))
sys.exit(1 if failures else 0)
