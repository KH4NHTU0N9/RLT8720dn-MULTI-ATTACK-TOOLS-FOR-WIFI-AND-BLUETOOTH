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

