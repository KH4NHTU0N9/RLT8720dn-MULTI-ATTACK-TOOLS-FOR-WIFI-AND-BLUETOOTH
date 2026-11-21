#include "web_ui.h"
#include "../config/app_config.h"
#include "../security/security_utils.h"

static const char kHtmlHeadStart[] =
    "<!DOCTYPE html><html lang='en'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='theme-color' content='#181a1f'>"
    "<title>RTL8720DN Tool</title><style>";

static const char kCssTheme[] =
    ":root{--bg:#16191f;--card:#21252e;--line:#313745;--txt:#e6e9ef;"
    "--mut:#94a0b2;--pri:#3b82f6;--red:#ef4444;--ok:#22c55e;--warn:#f59e0b}"
    "*{box-sizing:border-box}"
    "body{margin:0;background:var(--bg);color:var(--txt);font:14px/1.45 -apple-system,system-ui,sans-serif}"
    ".wrap{max-width:780px;margin:0 auto;padding:12px}"
    ".top{display:flex;flex-wrap:wrap;gap:8px;align-items:center;justify-content:space-between;margin-bottom:10px}"
    ".brand{font-size:18px;font-weight:700;display:flex;align-items:center;gap:8px}"
    ".pill{font-size:11px;padding:2px 8px;border-radius:99px;background:#2b3240;color:var(--mut)}";

static const char kCssLayout[] =
    ".note{background:#2d1f24;border-left:4px solid var(--red);padding:9px 12px;border-radius:6px;margin-bottom:10px;font-size:12px;color:#fecaca}"
    ".toast{padding:9px 12px;border-radius:6px;margin-bottom:10px;font-size:13px;font-weight:600}"
    ".toast.ok{background:#163024;border-left:4px solid var(--ok);color:#bbf7d0}"
    ".toast.warn{background:#332614;border-left:4px solid var(--warn);color:#fde68a}"
    ".toast.err{background:#35191d;border-left:4px solid var(--red);color:#fecaca}"
    ".toast.info{background:#18293d;border-left:4px solid var(--pri);color:#bfdbfe}"
    ".tabs{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:12px}"
    ".tab{flex:1 1 88px;padding:9px 10px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--mut);font-weight:600;cursor:pointer;text-align:center}"
    ".tab.on{background:var(--pri);border-color:var(--pri);color:#fff}"
    ".panel{display:none}.panel.on{display:block}"
    ".card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px;margin-bottom:12px}"
    ".chd{display:flex;justify-content:space-between;align-items:center;gap:8px;margin:0 0 10px;font-size:15px;font-weight:700}";

static const char kCssControls[] =
    ".row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}"
    ".fld{display:flex;flex-direction:column;gap:4px;flex:1 1 140px}"
    "label{font-size:12px;color:var(--mut)}"
    "input,select,textarea{width:100%;padding:8px 10px;border-radius:7px;border:1px solid var(--line);background:#14171c;color:var(--txt);font:inherit}"
    "textarea{resize:vertical;min-height:92px}"
    ".btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;padding:9px 14px;border:0;border-radius:7px;font:600 13px/1.2 inherit;color:#fff;background:#363d4d;cursor:pointer}"
    ".btn:hover{filter:brightness(1.12)}.btn:disabled{opacity:.6;cursor:wait}"
    ".btn.pri{background:var(--pri)}.btn.red{background:var(--red)}.btn.ok{background:#16a34a}.btn.sm{padding:6px 10px;font-size:12px}"
    ".tbl-wrap{overflow-x:auto}"
    "table{width:100%;border-collapse:collapse}"
    "th,td{padding:8px 6px;border-bottom:1px solid var(--line);text-align:left;vertical-align:middle}"
    "th{font-size:11px;text-transform:uppercase;color:var(--mut);letter-spacing:.04em}"
    ".ssid{font-weight:600;word-break:break-word}.meta{font-size:11px;color:var(--mut)}"
    ".hid{color:var(--warn);font-style:italic}"
    ".bar{width:96px;background:#14171c;border-radius:5px;overflow:hidden;border:1px solid var(--line)}"
    ".fill{padding:2px 6px;font-size:11px;font-weight:700;color:#fff;white-space:nowrap}"
    ".g{background:#16a34a}.o{background:#d97706}.r{background:#dc2626}"
    ".ck{width:20px;height:20px;accent-color:var(--pri);cursor:pointer}"
    ".acts{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}"
    ".acts .btn{flex:1 1 180px;padding:11px 14px}"
    ".kv{display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px solid var(--line)}.kv:last-child{border-bottom:0}";

static const char kScript[] =
    "<script>"
    "function tab(id){"
      "document.querySelectorAll('.panel').forEach(function(e){e.classList.toggle('on',e.id===id);});"
      "document.querySelectorAll('.tab').forEach(function(e){e.classList.toggle('on',e.getAttribute('data-t')===id);});"
      "try{sessionStorage.setItem('tab',id);}catch(e){}"
    "}"
    "function tog(cls,v){document.querySelectorAll('.'+cls).forEach(function(c){c.checked=v;});}"
    "function pickBle(m,t){"
      "var em=document.getElementById('b_mac'),et=document.getElementById('b_type');"
      "if(em)em.value=m;if(et)et.value=String(t);"
      "tab('p_ble');if(em)em.scrollIntoView({behavior:'smooth',block:'center'});"
    "}"
    "function fillSpam(v){var e=document.getElementById('s_list');if(e)e.value=v;}"
    "document.addEventListener('DOMContentLoaded',function(){"
      "try{var s=sessionStorage.getItem('tab');if(s&&document.getElementById(s))tab(s);}catch(e){}"
      "document.querySelectorAll('form').forEach(function(f){"
        "f.addEventListener('submit',function(ev){"
          "var btn=ev.submitter||f.querySelector('button[type=submit],input[type=submit]');"
          "if(btn){setTimeout(function(){btn.disabled=true;btn.textContent='Working...';},10);}"
        "});"
      "});"
    "});"
    "</script></body></html>";

static void _renderStatusBanner(WebStream& out, const StatusNotice& notice) {
    if (notice.kind == StatusKind::STATUS_NONE || notice.message[0] == '\0') return;
    const char* cls = "info";
    switch (notice.kind) {
        case StatusKind::STATUS_OK:   cls = "ok";   break;
        case StatusKind::STATUS_WARN: cls = "warn"; break;
        case StatusKind::STATUS_ERR:  cls = "err";  break;
        case StatusKind::STATUS_INFO:
        default:                      cls = "info"; break;
    }
    out.writeStr("<div class='toast ");
    out.writeStr(cls);
    out.writeStr("'>");
    out.writeHtmlEscaped(notice.message);
    out.writeStr("</div>");
}

static void _renderReasonOptions(WebStream& out, uint16_t active_reason) {
    static const uint16_t kReasons[] = {1, 2, 4, 6, 7, 8};
    static const char* const kLabels[] = {
        "1 - Unspecified",
        "2 - Auth invalid",
        "4 - Inactivity",
        "6 - Class 2 err",
        "7 - Class 3 err",
        "8 - STA leaving"
    };
    for (size_t i = 0; i < sizeof(kReasons) / sizeof(kReasons[0]); i++) {
        out.writeStr("<option value='");
        out.writeUInt((unsigned long)kReasons[i]);
        out.writeStr("'");
        if (active_reason == kReasons[i]) out.writeStr(" selected");
        out.writeStr(">");
        out.writeStr(kLabels[i]);
        out.writeStr("</option>");
    }
}

void webRenderScanRow(WebStream& out, const WiFiScanResult& ap, size_t idx, const char* group_cls) {
    const int color_score = ap.rssi + 130;
    const char* color_cls = (color_score < 50) ? "r" : (color_score < 70) ? "o" : "g";
    const int width = clampRssiPercent(ap.rssi);

    char bssid_str[18];
    formatMacAddress(ap.bssid, bssid_str, sizeof(bssid_str));

    out.writeStr("<tr><td><div class='ssid'>");
    if (ap.ssid.length() > 0) {
        out.writeHtmlEscaped(ap.ssid.c_str());
    } else {
        out.writeStr("<span class='hid'>*Hidden Network*</span>");
    }
    out.writeStr("</div><div class='meta'>CH ");
    out.writeUInt((unsigned long)ap.channel);
    out.writeStr(" &middot; ");
    out.writeStr(bssid_str);
    out.writeStr("</div></td><td><div class='bar'><div class='fill ");
    out.writeStr(color_cls);
    out.writeStr("' style='width:");
    out.writeInt((long)width);
    out.writeStr("%'>");
    out.writeInt((long)ap.rssi);
    out.writeStr(" dBm</div></div></td><td style='text-align:center'><input class='ck ");
    out.writeStr(group_cls ? group_cls : "g2");
    out.writeStr("' type='checkbox' name='target' value='");
    out.writeUInt((unsigned long)idx);
    out.writeStr("'></td></tr>");
}

static void _renderBandCard(WebStream& out, const char* title, const char* cls,
                            const std::vector<WiFiScanResult>& scan_results, bool want_5g) {
    int count = 0;
    for (size_t i = 0; i < scan_results.size(); i++) {
        if (is5GhzChannel(scan_results[i].channel) == want_5g) count++;
    }

    out.writeStr("<div class='card'><div class='chd'><span>");
    out.writeStr(title);
    out.writeStr(" (");
    out.writeInt((long)count);
    out.writeStr(")</span><label style='display:flex;align-items:center;gap:6px;cursor:pointer'><input class='ck' type='checkbox' onclick=\"tog('");
    out.writeStr(cls);
    out.writeStr("',this.checked)\">All</label></div><div class='tbl-wrap'><table><thead><tr><th>Network</th><th>Signal</th><th style='text-align:center'>Pick</th></tr></thead><tbody>");

    if (count == 0) {
        out.writeStr("<tr><td colspan='3' class='meta'>No access points found in this band.</td></tr>");
    } else {
        for (size_t i = 0; i < scan_results.size(); i++) {
            if (is5GhzChannel(scan_results[i].channel) == want_5g) {
                webRenderScanRow(out, scan_results[i], i, cls);
            }
        }
    }
    out.writeStr("</tbody></table></div></div>");
}

static void _renderWifiPanel(WebStream& out, const AppState& app,
                             const std::vector<WiFiScanResult>& scan_results) {
    out.writeStr("<div id='p_wifi' class='panel on'><form method='post' action='/deauth'>");
    _renderBandCard(out, "5 GHz Networks", "g5", scan_results, true);
    _renderBandCard(out, "2.4 GHz Networks", "g2", scan_results, false);

    out.writeStr("<div class='card'><div class='row'>"
                 "<div class='fld'><label>Frames / pulse</label><input type='number' name='frames' min='1' max='5000' value='");
    out.writeInt((long)app.config.frames_per_deauth);
    out.writeStr("'></div><div class='fld'><label>Deauth Reason</label><select name='reason'>");
    _renderReasonOptions(out, app.config.deauth_reason);
    out.writeStr("</select></div></div><div class='acts'>"
                 "<button class='btn red' type='submit' formaction='/deauth'>Start Deauth Attack</button>"
                 "<button class='btn pri' type='submit' formaction='/interfere'>Start Interference (Max 4)</button>"
                 "</div></div></form></div>");
}

static void _renderSpamPanel(WebStream& out, const AppState& app) {
    out.writeStr("<div id='p_spam' class='panel'><div class='card'>"
                 "<div class='chd'><span>Beacon Flooding (SSID Spammer)</span></div>"
                 "<div class='acts' style='margin:0 0 10px'>"
                 "<button class='btn sm' type='button' onclick=\"fillSpam('Free_Airport_WiFi\\nGuest_5G\\nCoffee_Free\\nPublic_Hotspot')\">Preset Demo</button>"
                 "<button class='btn sm' type='button' onclick=\"fillSpam('')\">Clear</button></div>"
                 "<form method='post' action='/spam'>"
                 "<div class='fld'><label>SSID List (one per line, max 64)</label>"
                 "<textarea id='s_list' name='ssids' placeholder='Free WiFi&#10;Test_AP'></textarea></div>"
                 "<div class='row' style='margin-top:8px'>"
                 "<div class='fld'><label>Beacons / SSID (1-20)</label><input type='number' name='count' min='1' max='20' value='");
    out.writeInt((long)app.config.beacons_per_ssid);
    out.writeStr("'></div><div class='fld'><label>Or Random SSIDs (0 = use list)</label>"
                 "<input type='number' name='random_count' min='0' max='64' value='0'></div></div>"
                 "<div class='acts'><button class='btn red' type='submit'>Start Beacon Flood</button></div>"
                 "</form></div></div>");
}
