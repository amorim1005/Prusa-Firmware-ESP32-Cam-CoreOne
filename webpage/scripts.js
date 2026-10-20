/* set the text of an element, a missing element is skipped */
function setText(id, value) {
	var element = document.getElementById(id);
	if (element) {
		element.textContent = value;
	}
}

/* GET request with a timeout, returns a promise with the response text */
function getText(url, timeout) {
	var controller = new AbortController();
	var timer = setTimeout(function() { controller.abort(); }, timeout);
	return fetch(url, { signal: controller.signal }).then(function(response) {
		clearTimeout(timer);
		return response.text();
	});
}

function get_data(val) {
	getText('json_input', 5000).then(function(data) {
		var obj = JSON.parse(data);

		document.title = obj.mdns;
		setText('refreshInterval', obj.upload_interval);
		
		if (val == "config") {
			setText('fingerprint', obj.fingerprint);
			document.getElementById('tokenid').value = obj.token;
			document.getElementById('refreshid').value = obj.refreshInterval;
			document.getElementById('photo_qualityid').value = obj.photoquality;
			/* show only the resolutions supported by the detected camera sensor */
			var supported = obj.framesize_supported.split(",");
			var framesizeOptions = document.getElementById('framesizeid').options;
			for (var i = 0; i < framesizeOptions.length; i++) {
				var available = supported.includes(framesizeOptions[i].value);
				framesizeOptions[i].hidden = !available;
				framesizeOptions[i].disabled = !available;
			}
			document.getElementById('framesizeid').value = obj.framesize;
			setText('camera_detected', obj.camera_detected);
			setText('camera_autofocus', obj.camera_autofocus);
			document.getElementById('brightnessid').value = obj.brightness;
			document.getElementById('contrastid').value = obj.contrast;
			document.getElementById('saturationid').value = obj.saturation;
			document.getElementById('image_rotationid').value = obj.image_rotation;
			document.getElementById('hmirrorid').checked = (obj.hmirror == "true");
			document.getElementById('vflipid').checked = (obj.vflip == "true");
			document.getElementById('lencid').checked = (obj.lensc == "true");
			document.getElementById('exposure_ctrlid').checked = (obj.exposure_ctrl == "true");
			document.getElementById('awbid').checked = (obj.awb == "true");
			document.getElementById('awb_gainid').checked = (obj.awb_gain == "true");
			document.getElementById('wb_modeid').value = obj.wb_mode;
			document.getElementById('bpcid').checked = (obj.bpc == "true");
			document.getElementById('wpcid').checked = (obj.wpc == "true");
			document.getElementById('raw_gamaid').checked = (obj.raw_gama == "true");
			document.getElementById('aec2id').checked = (obj.aec2 == "true");
			document.getElementById('ae_levelid').value = obj.ae_level;
			document.getElementById('aec_valueid').value = obj.aec_value;
			document.getElementById('gain_ctrlid').checked = (obj.gain_ctrl == "true");
			document.getElementById('agc_gainid').value = obj.agc_gain;
			document.getElementById("aec_value_value").innerText = obj.aec_value;
			document.getElementById('timelapsid').checked = (obj.timelaps == "true");
			document.getElementById('prusalinkid').checked = (obj.prusalink_enable == "true");
			document.getElementById('prusalink_hostid').value = obj.prusalink_host;
			document.getElementById('prusalink_userid').value = obj.prusalink_user;
			setText('status_prusalink', (obj.prusalink_enable == "true") ? "On" : "Off");
			setText('prusalink_status', obj.prusalink_status);
			setText('printer_state', obj.printer_state);
			setText('job_progress', (obj.printer_state == "PRINTING") ? obj.job_progress + " %" : "");
			setText('status_hmirror', (obj.hmirror == "true") ? "On" : "Off");
			setText('status_vflip', (obj.vflip == "true") ? "On" : "Off");
			setText('status_lensc', (obj.lensc == "true") ? "On" : "Off");
			setText('status_exposure_ctrl', (obj.exposure_ctrl == "true") ? "On" : "Off");
			setText('status_awb', (obj.awb == "true") ? "On" : "Off");
			setText('status_awb_gain', (obj.awb_gain == "true") ? "On" : "Off");
			setText('status_bpc', (obj.bpc == "true") ? "On" : "Off");
			setText('status_wpc', (obj.wpc == "true") ? "On" : "Off");
			setText('status_raw_gama', (obj.raw_gama == "true") ? "On" : "Off");
			setText('status_aec2', (obj.aec2 == "true") ? "On" : "Off");
			setText('status_gain_ctrl', (obj.gain_ctrl == "true") ? "On" : "Off");
			setText('status_timelaps', (obj.timelaps == "true") ? "On" : "Off");
			sliderCheck();
		}

		if (val == "auth") {
			document.getElementById('authid').checked = (obj.auth == "true");
			setText('status_auth', (obj.auth == "true") ? "On" : "Off");
			document.getElementById('auth_username').value = obj.auth_username;
		}

		if (val == "wifi") {
			document.getElementById('serviceapid').checked = (obj.serviceap == "true");
			setText('status_serviceap', (obj.serviceap == "true") ? "On" : "Off");
			setText('ssid', obj.ssid);
			setText('rssi', obj.rssi);
			setText('rssi_percentage', obj.rssi_percentage);
			setText('ip', obj.ip);
			setText('mdns', obj.mdns);
			setText('wifi_network_status', obj.wifi_network_status);
			document.getElementById('ipcfgid').value = obj.ip_cfg;
			document.getElementById('net_ip_id').value = obj.net_ip;
			document.getElementById('net_mask_id').value = obj.net_mask;
			document.getElementById('net_gw_id').value = obj.net_gw;
			document.getElementById('net_dns_id').value = obj.net_dns;

			if (!document.querySelector('#main-wifi-signal wifi_img')) {
				var wifi_img = document.createElement('wifi_img');
				wifi_img.width = 19;
				wifi_img.height = 12;
				wifi_img.src = getIconPath(obj.rssi);
				document.getElementById('main-wifi-signal').appendChild(wifi_img);
			}
		}

		if (val == "system") {
			setText('uptime', obj.uptime);
			setText('sw_ver', obj.sw_ver);
			setText('sw_build', obj.sw_build);
			setText('last_upload_status', obj.last_upload_status);
			setText('wifi_mode', obj.wifi_mode);
			setText('service_ap_ssid', obj.service_ap_ssid);
			setText('sd_status', obj.sd_status);
			setText('sd_total', obj.sd_total);
			setText('sd_free_p', obj.sd_free_p);
			setText('sd_used_p', obj.sd_used_p);
			setText('mcu_temp', obj.mcu_temp);
			setText('upload_interval', obj.upload_interval);
			setText('health_status', obj.health_status);
			setText('upload_ok', obj.upload_ok);
			setText('upload_failed', obj.upload_failed);
			setText('unplanned_restarts', obj.unplanned_restarts);
			setText('last_restart', obj.last_restart);
			setText('mcu_temp_max', obj.mcu_temp_max);
			setText('mem_internal', obj.mem_internal);
			setText('mem_psram', obj.mem_psram);

			var sd_free_prog = document.getElementById("progress_bar_sd_free");
			sd_free_prog.style.width = obj.sd_free_p + "%";
			sd_free_prog.innerHTML = obj.sd_free_p + "%";

			var sd_free_prog = document.getElementById("progress_bar_sd_used");
			sd_free_prog.style.width = obj.sd_used_p + "%";
			sd_free_prog.innerHTML = obj.sd_used_p + "%";

			document.getElementById('mdnsid').value = obj.mdns;
			document.getElementById('loglevelid').value = obj.log_level;
		}
	}).catch(function(error) {
		console.log("json timeout or error: " + error);
	});
}

function sliderCheck() {
	var ranges = document.querySelectorAll(".slider");
	ranges.forEach(function(range) {
		var percent = (range.value - range.min) / (range.max - range.min) * 100;
		var gradient = "linear-gradient(to right, #FA6831 " + percent + "%, #d3d3d3 " + percent + "%)";
		range.style.background = gradient;
		range.oninput = function() {
			var percent = (this.value - this.min) / (this.max - this.min) * 100;
			var gradient = "linear-gradient(to right, #FA6831 " + percent + "%, #d3d3d3 " + percent + "%)";
			this.style.background = gradient;
		}
	});
}

function getIconPath(rssi) {
	let path;
	if (rssi == 0) {
		path = 'wifi-icon-0.svg';
	} else if (rssi <= -70) {
		path = 'wifi-icon-1.svg';
	} else if (rssi > -70 && rssi <= -60) {
		path = 'wifi-icon-2.svg';
	} else if (rssi > -60 && rssi <= -50) {
		path = 'wifi-icon-3.svg';
	} else {
		path = 'wifi-icon-4.svg';
	}
	return path;
}

var OpenImageclickCount = 0;

function openImage() {
	var img = document.getElementById("photo");
	if (OpenImageclickCount % 2 == 0) {
		img.style.position = "fixed";
		img.style.top = "5%";
		img.style.left = "5%";
		img.style.width = "auto";
		img.style.height = "auto";
		img.style.maxWidth = "100%";
		img.style.maxHeight = "90%";
		img.style.zIndex = "9999";
	} else {
		img.style.position = "";
		img.style.top = "";
		img.style.left = "";
		img.style.width = "";
		img.style.height = "";
		img.style.zIndex = "";
	}
	OpenImageclickCount++;
}

function actionButton(url, reload, msg) {
    var xhr = new XMLHttpRequest();
    
	if (msg != '') {
		alert(msg);
	}

    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200) {
            if (reload == true) {
                setTimeout(function() {
                    location.reload();
                }, 200);
            }
        }
    };

    xhr.open('GET', url, true);
    xhr.send();
}

function setActive(link) {
	var links = document.querySelectorAll('#links li a');
	links.forEach(function(item) {
		item.classList.remove('active');
	});
	link.classList.add('active');
}

var links = document.querySelectorAll('#links li a');
links.forEach(function(link) {
	link.addEventListener('click', function() {
		setActive(link);
	});
});

function addClickListener(id) {
    var link = document.getElementById(id);
    if (!link.hasOwnProperty('clickListener')) {
        link.addEventListener('click', function(event) {
            event.preventDefault();
            window.open(link.href, '_blank');
        });
        link.clickListener = true;
    }
}

/* wifi page */
function setWifi(val_ssid, val_pass) {
	var xmlHttp = new XMLHttpRequest();
	xmlHttp.open("GET", "wifi_cfg?wifi_ssid=" +  encodeURIComponent(val_ssid) + "&wifi_pass=" + encodeURIComponent(val_pass), false);
	xmlHttp.send(null);
	alert(xmlHttp.responseText);
	get_data("wifi");
}

function setWifiNet(val_ip, val_mask, val_gw, val_dns) {	
	var xmlHttp = new XMLHttpRequest();
	xmlHttp.open("GET", "wifi_net_cfg?ip=" +  encodeURIComponent(val_ip) + "&mask=" + encodeURIComponent(val_mask) + "&gw=" + encodeURIComponent(val_gw) + "&dns=" + encodeURIComponent(val_dns), false);
	xmlHttp.send(null);
	alert(xmlHttp.responseText);
	get_data("wifi");
}

function scanWifi() {
	var xmlHttp = new XMLHttpRequest();
	xmlHttp.open("GET", "wifi_scan?", false);
	xmlHttp.send(null);
	alert(xmlHttp.responseText);
	get_data("wifi");
	setTimeout(function() {
		GetDataAndPrintTableWiFi();
	}, 8000);
}

function GetDataAndPrintTableWiFi() {
	var table = document.getElementById('wifi_ntw');
	/* keep the header row */
	while (table.rows.length > 1) {
		table.deleteRow(1);
	}
	getText('json_wifi', 15000).then(function(text) {
		var data = JSON.parse(text);
		for (var i = 0; i < data.length; i++) {
			var row = table.insertRow(-1);
			var signal = document.createElement('div');
			signal.style.display = 'flex';
			signal.style.alignItems = 'center';
			var img = document.createElement('img');
			img.src = getIconPath(data[i].rssi);
			signal.append(img, data[i].rssi_percentage + "% / " + data[i].rssi + "dBm");
			row.insertCell(-1).textContent = data[i].ssid;
			row.insertCell(-1).appendChild(signal);
			row.insertCell(-1).textContent = data[i].channel;
			row.insertCell(-1).textContent = data[i].encryption;
		}
	}).catch(function(error) {
		console.log('Error: ' + error);
	});
}

/* PrusaLink cfg, the password is sent only when the user typed a new one */
function setPrusaLink() {
	var url = "set_prusalink?enable=" + document.getElementById('prusalinkid').checked
		+ "&host=" + encodeURIComponent(document.getElementById('prusalink_hostid').value)
		+ "&user=" + encodeURIComponent(document.getElementById('prusalink_userid').value)
		+ "&password=" + encodeURIComponent(document.getElementById('prusalink_passwordid').value);
	var xmlHttp = new XMLHttpRequest();
	xmlHttp.open("GET", url, false);
	xmlHttp.send(null);
	alert(xmlHttp.responseText);
	document.getElementById('prusalink_passwordid').value = "";
	get_data("config");
}

/* auth page */
function setAuth(val_name, val_pass) {
	var xmlHttp = new XMLHttpRequest();
	xmlHttp.open("GET", "basicauth_cfg?" + "auth_username=" + encodeURIComponent(val_name) + "&auth_password=" + encodeURIComponent(val_pass), false);
	xmlHttp.send(null);
	alert(xmlHttp.responseText);
	get_data("auth");
}

function changeValue(val, url, reload, msg) {
	var xmlHttp = new XMLHttpRequest();
	xmlHttp.open("GET", url + val, false);
	xmlHttp.send(null);
	if ((url == "set_int?refresh=") || (url == "set_token?token=") || (url == "set_mdns?mdns=") || (url == "set_int?ipcfg=") ||
		xmlHttp.responseText.startsWith("ERROR")) {
		alert(xmlHttp.responseText);
	}
	get_data(reload);
}

function togglePasswordVisibility() {
	const passwordInput = document.getElementById("auth_password");
	const eyeIcon = document.getElementById("eye-icon");
	if (passwordInput.getAttribute("type") === "password") {
		passwordInput.setAttribute("type", "text");
		passwordInput.classList.add("reveal");
		eyeIcon.src = "eye-slash.svg";
		eyeIcon.alt = "Hide Password";
	} else {
		passwordInput.setAttribute("type", "password");
		passwordInput.classList.remove("reveal");
		eyeIcon.src = "eye.svg";
		eyeIcon.alt = "Show Password";
	}
}

/* system page: firmware update from file */
function uploadFirmware() {
	var file = document.getElementById('firmwareInput').files[0];
	var status = document.getElementById('firmwareStatus');
	var bar = document.getElementById('firmwareProgress');

	if (!file) {
		status.innerText = 'No file selected';
		return;
	}
	if (!confirm('Install ' + file.name + '? The camera restarts after the upload.')) {
		return;
	}

	var formData = new FormData();
	formData.append('firmware', file, file.name);

	var xhr = new XMLHttpRequest();
	xhr.open('POST', 'upload_firmware', true);
	xhr.upload.onprogress = function(e) {
		if (e.lengthComputable) {
			var p = Math.round(e.loaded * 100 / e.total);
			bar.style.width = p + '%';
			bar.innerHTML = p + '%';
		}
	};
	xhr.onload = function() {
		status.innerText = xhr.responseText;
		if (xhr.status == 200) {
			setTimeout(function() { location.reload(); }, 20000);
		}
	};
	xhr.onerror = function() {
		status.innerText = 'Upload failed: connection error';
	};
	status.innerText = 'Uploading...';
	xhr.send(formData);
}

function validatePasswords() {
    var password = document.getElementById("auth_password").value;
    var confirmPassword = document.getElementById("auth_password_confirm").value;
    var saveButton = document.querySelector(".btn_save_a");
    var passwordStatus = document.getElementById("pass_match");

    if (password === confirmPassword) {
        passwordStatus.innerHTML = "✔️";
        saveButton.disabled = false;
    } else {
        passwordStatus.innerHTML = "❌";
        saveButton.disabled = true;
    }
}

if (document.getElementById("auth_password")) {
	document.getElementById("auth_password").addEventListener("input", validatePasswords);
}

if (document.getElementById("auth_password_confirm")) {
	document.getElementById("auth_password_confirm").addEventListener("input", validatePasswords);
}

if ((document.getElementById("auth_password")) && (document.getElementById("auth_password_confirm"))) {
	validatePasswords();
}

/* the section after the button opens and closes */
function setupCollapsible(selector) {
	document.querySelectorAll(selector).forEach(function(button) {
		button.addEventListener('click', function() {
			button.classList.toggle('active');
			var content = button.parentElement.nextElementSibling;
			content.style.display = (getComputedStyle(content).display === 'block') ? 'none' : 'block';
		});
	});
}

function setupCollapsibleButtons() {
	setupCollapsible('.btn_collapsible');
}

function setupCollapsibleButtonsWiFi() {
	setupCollapsible('.btn_collapsible_wifi');
}
