/* eslint-env browser, node */
(function (factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory;
  else factory(window, document);
}(function installPreviewVisibility(win, doc) {
  var Native = win.WebSocket;
  if (!Native || Native.pixelitPreviewVisibility) return;
  var viewers = [];
  var compactPrefix = '{"liveview64":"';
  var hex = '0123456789ABCDEF';
  function expandPreview(event) {
    var encoded = event.data;
    if (encoded.indexOf(compactPrefix) !== 0 || encoded.slice(-2) !== '"}') return;
    var rgb = win.atob(encoded.slice(compactPrefix.length, -2));
    var pixels = '';
    for (var i = 0; i < rgb.length; i++) {
      var byte = rgb.charCodeAt(i);
      pixels += hex[byte >> 4] + hex[byte & 15];
    }
    Object.defineProperty(event, 'data', { value: '{"liveview":"' + pixels + '"}' });
  }
  function update(entry, enabled) {
    if (!entry.supported || entry.socket.readyState !== Native.OPEN || entry.last === enabled) return;
    try {
      var preview = { enabled: enabled };
      if (entry.compact) preview.compact = true;
      entry.socket.send(JSON.stringify({ preview: preview }));
      entry.last = enabled;
    } catch (error) { /* Retry only on the next existing event, never poll. */ }
  }
  function visible() {
    viewers.forEach(function (entry) {
      var newest;
      for (var i = viewers.length - 1; i >= 0; i--) {
        var candidate = viewers[i];
        if (candidate.key === entry.key && candidate.supported && candidate.socket.readyState === Native.OPEN) {
          newest = candidate;
          break;
        }
      }
      update(entry, !doc.hidden && entry === newest);
    });
  }
  function Wrapped(url, protocols) {
    var socket = protocols === undefined ? new Native(url) : new Native(url, protocols);
    var target;
    try { target = new win.URL(url, win.location.href); } catch (error) { return socket; }
    if (target.port !== '81') return socket;
    var entry = { socket: socket, key: target.origin, supported: false, last: undefined };
    viewers.push(entry);
    socket.addEventListener('message', function (event) {
      if (typeof event.data !== 'string') return;
      try {
        if (entry.compact && event.data.indexOf(compactPrefix) === 0) {
          expandPreview(event);
          return;
        }
        if (event.data.indexOf('"sysinfo"') === -1) return;
        var value = JSON.parse(event.data);
        if (value.sysinfo && value.sysinfo.previewControl === true) {
          entry.supported = true;
          entry.compact = value.sysinfo.previewCompact === true;
          visible();
        }
      } catch (error) { /* The ordinary app still handles its own messages. */ }
    });
    socket.addEventListener('close', function () {
      var index = viewers.indexOf(entry);
      if (index !== -1) viewers.splice(index, 1);
      visible();
    });
    return socket;
  }
  Wrapped.prototype = Native.prototype;
  ['CONNECTING', 'OPEN', 'CLOSING', 'CLOSED'].forEach(function (key) {
    Object.defineProperty(Wrapped, key, { value: Native[key] });
  });
  Wrapped.pixelitPreviewVisibility = true;
  win.WebSocket = Wrapped;
  doc.addEventListener('visibilitychange', visible);
  win.addEventListener('pageshow', visible);
  win.addEventListener('pagehide', function () {
    viewers.forEach(function (entry) { update(entry, false); });
  });
}));
