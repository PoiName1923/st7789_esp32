let csrf = '';
const $ = selector => document.querySelector(selector);
async function api(url, options = {}) {
  options.headers = {...(options.headers || {}), 'Content-Type': 'application/json'};
  if (csrf) options.headers['X-CSRF-Token'] = csrf;
  const response = await fetch(url, options);
  const json = await response.json();
  if (!response.ok) throw Error(json.error || 'Yêu cầu thất bại');
  return json;
}
function message(text) { $('#message').textContent = text; }
async function refresh() {
  const status = await api('/api/status');
  $('#network').textContent = status.network;
  $('#temperature').textContent = status.temperature == null ? '—' : `${status.temperature} °C`;
  $('#humidity').textContent = status.humidity == null ? '—' : `${status.humidity} %`;
  $('#error').textContent = status.last_error || 'Không có';
}
async function loadSettings() {
  const s = await api('/api/settings');
  const d = $('#displayForm').elements, n = $('#networkForm').elements;
  d.fps.value = s.fps; d.faceColor.value = `#${s.face_color.toString(16).padStart(6, '0')}`;
  d.screenSwitch.value = s.screen_switch; d.infoDuration.value = s.info_duration;
  d.rotation.value = s.rotation; d.bgr.checked = s.bgr; d.invert.checked = s.invert;
  d.offsetX.value = s.offset_x; d.offsetY.value = s.offset_y;
  n.ssid.value = s.ssid; n.latitude.value = s.latitude; n.longitude.value = s.longitude;
  n.timezone.value = s.timezone; n.weatherInterval.value = s.weather_interval;
}
$('#loginForm').onsubmit = async event => {
  event.preventDefault();
  try {
    const result = await api('/api/login', {method:'POST', body:JSON.stringify({password:$('#password').value})});
    csrf = result.csrf; $('#login').hidden = true; $('#panel').hidden = false;
    await Promise.all([refresh(), loadSettings()]);
  } catch (error) { $('#login p').textContent = error.message; }
};
document.querySelectorAll('[data-screen]').forEach(button => button.onclick = () =>
  api('/api/command', {method:'POST', body:JSON.stringify({screen:+button.dataset.screen})})
    .then(() => message('Đã đổi màn hình')).catch(error => message(error.message)));
$('#animation').onchange = event => api('/api/command', {method:'POST', body:JSON.stringify({animation:+event.target.value})}).catch(error => message(error.message));
$('#displayForm').onsubmit = event => {
  event.preventDefault(); const f = new FormData(event.target);
  api('/api/settings', {method:'POST', body:JSON.stringify({fps:+f.get('fps'), face_color:parseInt(f.get('faceColor').slice(1),16), screen_switch:+f.get('screenSwitch'), info_duration:+f.get('infoDuration'), rotation:+f.get('rotation'), bgr:f.get('bgr')==='on', invert:f.get('invert')==='on', offset_x:+f.get('offsetX'), offset_y:+f.get('offsetY')})})
    .then(result => message(result.restart_required ? 'Đã lưu; hãy khởi động lại robot' : 'Đã lưu')).catch(error => message(error.message));
};
$('#networkForm').onsubmit = event => {
  event.preventDefault(); if (!confirm('Kết nối có thể bị ngắt. Tiếp tục?')) return;
  const f = new FormData(event.target);
  api('/api/settings', {method:'POST', body:JSON.stringify({ssid:f.get('ssid'), wifi_password:f.get('wifiPassword'), open_network:f.get('openNetwork')==='on', ap_password:f.get('apPassword'), latitude:f.get('latitude'), longitude:f.get('longitude'), timezone:f.get('timezone'), weather_interval:+f.get('weatherInterval')})})
    .then(() => message('Đã lưu. Hãy kết nối lại nếu cần.')).catch(error => message(error.message));
};
$('#adminForm').onsubmit = event => {
  event.preventDefault(); const f = new FormData(event.target);
  api('/api/password', {method:'POST', body:JSON.stringify({old_password:f.get('oldPassword'), new_password:f.get('newPassword')})})
    .then(() => { csrf=''; location.reload(); }).catch(error => message(error.message));
};
setInterval(() => { if (!$('#panel').hidden) refresh().catch(() => {}); }, 5000);
