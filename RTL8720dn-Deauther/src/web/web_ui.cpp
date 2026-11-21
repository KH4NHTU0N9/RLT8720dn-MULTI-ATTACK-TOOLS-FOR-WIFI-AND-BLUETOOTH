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

