(function () {
  "use strict";

  var root = document.querySelector(".error-404");
  if (!root) return;

  var output = root.querySelector("[data-output]");
  var pathSpan = root.querySelector("[data-path]");
  var input = root.querySelector("[data-input]");
  var led = root.querySelector(".error-404__led");
  var suggestionsBox = root.querySelector("[data-suggestions]");
  var routesNode = document.getElementById("esphome-404-routes");
  var routes = [];
  try {
    routes = JSON.parse(routesNode.textContent || "[]");
  } catch (_) {
    routes = [];
  }

  var currentPath = window.location.pathname + window.location.search;
  if (pathSpan) {
    pathSpan.textContent =
      currentPath.length > 60 ? currentPath.slice(0, 57) + "..." : currentPath;
  }

  // --- Suggestion engine -----------------------------------------------

  function tokenize(str) {
    return str
      .toLowerCase()
      .replace(/\.html?$/, "")
      .split(/[^a-z0-9]+/)
      .filter(function (t) {
        return t.length > 1 && t !== "index";
      });
  }

  function editDistance(a, b) {
    if (a === b) return 0;
    if (!a.length) return b.length;
    if (!b.length) return a.length;
    var prev = new Array(b.length + 1);
    var curr = new Array(b.length + 1);
    for (var j = 0; j <= b.length; j++) prev[j] = j;
    for (var i = 1; i <= a.length; i++) {
      curr[0] = i;
      for (var k = 1; k <= b.length; k++) {
        var cost = a.charAt(i - 1) === b.charAt(k - 1) ? 0 : 1;
        curr[k] = Math.min(prev[k] + 1, curr[k - 1] + 1, prev[k - 1] + cost);
      }
      var tmp = prev;
      prev = curr;
      curr = tmp;
    }
    return prev[b.length];
  }

  function suggest(pathname, limit) {
    var tokens = tokenize(pathname);
    if (!tokens.length) return [];

    var lastSeg = pathname
      .replace(/\/$/, "")
      .split("/")
      .pop()
      .replace(/\.html?$/, "")
      .toLowerCase();

    var sectionHint = pathname.split("/").filter(Boolean)[0] || "";

    var scored = routes
      .map(function (r) {
        var url = r.url.toLowerCase();
        var title = (r.title || "").toLowerCase();
        var score = 0;

        if (sectionHint && url.indexOf("/" + sectionHint + "/") === 0) {
          score += 2;
        }

        for (var i = 0; i < tokens.length; i++) {
          var t = tokens[i];
          if (url.indexOf(t) !== -1) score += 3;
          if (title.indexOf(t) !== -1) score += 2;
        }

        if (lastSeg) {
          var urlSlug = url
            .replace(/\/$/, "")
            .split("/")
            .pop();
          var titleSlug = title.replace(/\s+/g, "-");
          var best = Math.min(
            editDistance(lastSeg, urlSlug),
            editDistance(lastSeg, titleSlug),
          );
          if (best === 0) score += 10;
          else if (best <= 2) score += 6;
          else if (best <= 4) score += 3;
        }

        return { route: r, score: score };
      })
      .filter(function (entry) {
        return entry.score > 0;
      });

    scored.sort(function (a, b) {
      if (b.score !== a.score) return b.score - a.score;
      return a.route.url.length - b.route.url.length;
    });

    var seen = {};
    var out = [];
    for (var n = 0; n < scored.length && out.length < limit; n++) {
      var url = scored[n].route.url;
      if (seen[url]) continue;
      seen[url] = true;
      out.push(scored[n].route);
    }
    return out;
  }

  // --- Terminal output helpers -----------------------------------------

  function appendLine(html, className) {
    if (!output) return;
    var div = document.createElement("div");
    if (className) div.className = className;
    div.innerHTML = html;
    output.appendChild(div);
  }

  function appendText(text, className) {
    if (!output) return;
    var div = document.createElement("div");
    if (className) div.className = className;
    div.textContent = text;
    output.appendChild(div);
  }

  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&#39;");
  }

  function scrollToBottom() {
    var body = root.querySelector(".error-404__terminal-body");
    if (body) body.scrollTop = body.scrollHeight;
  }

  // --- Render initial suggestions --------------------------------------

  var initialSuggestions = suggest(window.location.pathname, 3);
  if (initialSuggestions.length) {
    appendLine(
      '<span class="error-404__log-info">[I]</span> ' +
        '<span class="error-404__log-tag">[suggest:042]</span> ' +
        '<span class="error-404__log-text">Did you mean one of these?</span>',
    );
    initialSuggestions.forEach(function (r) {
      appendLine(
        '<span class="error-404__log-suggest">  →</span> ' +
          '<a class="error-404__log-link" href="' +
          escapeHtml(r.url) +
          '">' +
          escapeHtml(r.url) +
          "</a> " +
          '<span class="error-404__log-tag">' +
          escapeHtml(r.title) +
          "</span>",
      );
    });

    if (suggestionsBox) {
      initialSuggestions.forEach(function (r) {
        var a = document.createElement("a");
        a.className = "error-404__suggestion";
        a.href = r.url;
        a.innerHTML =
          '<span class="error-404__suggestion-title">' +
          escapeHtml(r.title) +
          "</span>" +
          '<span class="error-404__suggestion-url">' +
          escapeHtml(r.url) +
          "</span>";
        suggestionsBox.appendChild(a);
      });
      suggestionsBox.hidden = false;
    }
  }

  // --- Commands --------------------------------------------------------

  var history = [];
  var historyIndex = -1;

  function navigate(url) {
    window.location.href = url;
  }

  function fuzzyFindRoute(query) {
    var q = query.toLowerCase().trim();
    if (!q) return null;
    var exact = routes.find(function (r) {
      return (
        r.url.toLowerCase() === q ||
        r.url.toLowerCase() === "/" + q + "/" ||
        r.url.toLowerCase() === "/" + q ||
        r.title.toLowerCase() === q
      );
    });
    if (exact) return exact;
    var matches = suggest(q, 1);
    return matches[0] || null;
  }

  var commands = {
    help: function () {
      appendLine(
        '<span class="error-404__log-text">Available commands:</span>',
      );
      var list = [
        ["help", "show this message"],
        ["ls [section]", "list sections or pages in a section"],
        ["cd &lt;path&gt;", "navigate to a path"],
        ["open &lt;name&gt;", "fuzzy-open a page by name"],
        ["search &lt;query&gt;", "search the docs"],
        ["home", "go back to the homepage"],
        ["pwd", "print the current (broken) path"],
        ["clear", "clear the console"],
        ["reboot", "reload the page"],
        ["blink", "re-trigger the LED morse blink"],
        ["whoami", "who am I?"],
        ["uptime", "how long has the ESP been up?"],
        ["echo &lt;text&gt;", "print text"],
      ];
      list.forEach(function (row) {
        appendLine(
          '<span class="error-404__log-suggest">  •</span> ' +
            '<span class="error-404__log-link">' +
            row[0] +
            "</span> " +
            '<span class="error-404__log-tag">— ' +
            row[1] +
            "</span>",
        );
      });
      appendLine(
        '<span class="error-404__log-tag">  (plus a handful of easter eggs…)</span>',
      );
    },

    ls: function (args) {
      var section = (args[0] || "").toLowerCase();
      if (!section) {
        var sections = {};
        routes.forEach(function (r) {
          var parts = r.url.split("/").filter(Boolean);
          if (parts.length) sections[parts[0]] = true;
        });
        var keys = Object.keys(sections).sort();
        appendLine(
          '<span class="error-404__log-text">Top-level sections:</span>',
        );
        keys.forEach(function (s) {
          appendLine(
            '<span class="error-404__log-suggest">  →</span> ' +
              '<a class="error-404__log-link" href="/' +
              escapeHtml(s) +
              '/">/' +
              escapeHtml(s) +
              "/</a>",
          );
        });
        appendLine(
          '<span class="error-404__log-tag">  Tip: <code>ls components</code> to list components.</span>',
        );
        return;
      }
      var prefix = "/" + section + "/";
      var inSection = routes.filter(function (r) {
        return r.url.indexOf(prefix) === 0;
      });
      if (!inSection.length) {
        appendText("ls: no such section: " + section, "error-404__log-err");
        return;
      }
      appendLine(
        '<span class="error-404__log-text">' +
          inSection.length +
          " page(s) in /" +
          escapeHtml(section) +
          "/:</span>",
      );
      inSection.forEach(function (r) {
        appendLine(
          '<span class="error-404__log-suggest">  →</span> ' +
            '<a class="error-404__log-link" href="' +
            escapeHtml(r.url) +
            '">' +
            escapeHtml(r.url) +
            "</a> " +
            '<span class="error-404__log-tag">' +
            escapeHtml(r.title) +
            "</span>",
        );
      });
    },

    cd: function (args) {
      var target = args.join(" ").trim();
      if (!target || target === "~" || target === "/" || target === "..") {
        navigate("/");
        return;
      }
      if (target.indexOf("/") === 0) {
        navigate(target.endsWith("/") ? target : target + "/");
        return;
      }
      var hit = fuzzyFindRoute(target);
      if (hit) {
        appendText("cd: " + hit.url, "error-404__log-text");
        navigate(hit.url);
      } else {
        appendText("cd: no match for: " + target, "error-404__log-err");
      }
    },

    open: function (args) {
      var q = args.join(" ").trim();
      if (!q) {
        appendText("open: missing argument", "error-404__log-err");
        return;
      }
      var hit = fuzzyFindRoute(q);
      if (hit) {
        appendText("opening " + hit.url, "error-404__log-text");
        navigate(hit.url);
      } else {
        appendText("open: no page matches: " + q, "error-404__log-err");
      }
    },

    search: function (args) {
      var q = args.join(" ").trim();
      if (!q) {
        appendText("search: missing query", "error-404__log-err");
        return;
      }
      var btn = document.querySelector("button[data-open-modal]");
      if (btn) {
        btn.click();
        setTimeout(function () {
          var box = document.querySelector(
            'dialog[open] input[type="search"], dialog[open] input[type="text"]',
          );
          if (box) {
            box.value = q;
            box.dispatchEvent(new Event("input", { bubbles: true }));
            box.focus();
          }
        }, 60);
      } else {
        navigate("/?search=" + encodeURIComponent(q));
      }
    },

    home: function () {
      navigate("/");
    },

    pwd: function () {
      appendText(
        window.location.pathname + window.location.search,
        "error-404__log-text",
      );
    },

    clear: function () {
      if (output) output.innerHTML = "";
    },

    reboot: function () {
      appendText(
        "Rebooting ESPHome in 1 second…",
        "error-404__log-warn",
      );
      setTimeout(function () {
        window.location.reload();
      }, 800);
    },

    restart: function () {
      commands.reboot();
    },

    blink: function () {
      if (!led) return;
      led.style.animation = "none";
      // force reflow so the animation restarts from 0
      void led.offsetWidth;
      led.style.animation = "";
      appendText("LED pulse restarted.", "error-404__log-text");
    },

    whoami: function () {
      appendText("esphomer", "error-404__log-text");
    },

    uptime: function () {
      var secs = Math.floor(performance.now() / 1000);
      var m = Math.floor(secs / 60);
      var s = secs % 60;
      appendText(
        "up " + m + "m " + s + "s, load average: 0.04, 0.01, 0.00",
        "error-404__log-text",
      );
    },

    echo: function (args) {
      appendText(args.join(" "), "error-404__log-text");
    },

    cat: function (args) {
      var file = (args[0] || "").toLowerCase();
      if (file === "secrets.yaml") {
        appendLine(
          '<span class="error-404__log-err">cat: secrets.yaml: Permission denied</span>',
        );
        appendLine(
          '<span class="error-404__log-tag">  (nice try — secrets stay secret)</span>',
        );
      } else if (file === "config.yaml" || file === "esphome.yaml") {
        appendLine(
          '<span class="error-404__log-text">esphome:</span>',
        );
        appendLine(
          '<span class="error-404__log-text">  name: docs-server</span>',
        );
        appendLine(
          '<span class="error-404__log-text">  friendly_name: esphome.io</span>',
        );
        appendLine(
          '<span class="error-404__log-text">wifi:</span>',
        );
        appendLine(
          '<span class="error-404__log-text">  ssid: !secret wifi_ssid</span>',
        );
        appendLine(
          '<span class="error-404__log-text">  password: !secret wifi_password</span>',
        );
        appendLine(
          '<span class="error-404__log-text">logger:</span>',
        );
      } else if (file) {
        appendText(
          "cat: " + args[0] + ": No such file or directory",
          "error-404__log-err",
        );
      } else {
        appendText("cat: missing operand", "error-404__log-err");
      }
    },

    sudo: function () {
      appendText(
        "sudo: permission denied — no root on ESP",
        "error-404__log-err",
      );
    },

    rm: function (args) {
      var joined = args.join(" ");
      if (joined.indexOf("-rf") !== -1 || joined.indexOf("/") !== -1) {
        appendText(
          "rm: refusing to remove '/': filesystem is flash, and flash is forever.",
          "error-404__log-err",
        );
      } else {
        appendText("rm: missing operand", "error-404__log-err");
      }
    },

    esphome: function (args) {
      var sub = (args[0] || "").toLowerCase();
      if (sub === "run" || sub === "compile") {
        var lines = [
          "INFO Reading configuration /config/esphome.yaml...",
          "INFO Detected timezone 'UTC'",
          "INFO Generating C++ source...",
          "INFO Compiling app...",
          "Processing docs-server (board: esp32dev; framework: arduino)",
          "RAM:   [==        ]  17.3% (used 56728 bytes)",
          "Flash: [===       ]  27.9% (used 923456 bytes)",
          "INFO Successfully compiled program.",
          "INFO ...but the page you wanted still doesn't exist. 😉",
        ];
        var i = 0;
        (function next() {
          if (i >= lines.length) return;
          appendText(lines[i++], "error-404__log-text");
          scrollToBottom();
          setTimeout(next, 140);
        })();
      } else if (sub === "logs") {
        commands.blink();
      } else {
        appendText(
          "Usage: esphome [run|logs|compile]",
          "error-404__log-text",
        );
      }
    },

    ping: function (args) {
      var host = args[0] || "esphome.io";
      var count = 4;
      var i = 0;
      (function tick() {
        if (i >= count) {
          appendText(
            "--- " +
              host +
              " ping statistics --- " +
              count +
              " packets transmitted, " +
              count +
              " received, 0% packet loss",
            "error-404__log-text",
          );
          return;
        }
        i++;
        var t = (Math.random() * 20 + 5).toFixed(1);
        appendText(
          "64 bytes from " +
            host +
            ": icmp_seq=" +
            i +
            " ttl=64 time=" +
            t +
            " ms",
          "error-404__log-text",
        );
        scrollToBottom();
        setTimeout(tick, 320);
      })();
    },

    date: function () {
      appendText(new Date().toString(), "error-404__log-text");
    },
  };

  commands["?"] = commands.help;
  commands["cls"] = commands.clear;
  commands["exit"] = commands.home;
  commands["quit"] = commands.home;

  function run(line) {
    var trimmed = line.trim();
    if (!trimmed) return;

    appendLine(
      '<span class="error-404__log-info">esphome</span>' +
        '<span class="error-404__prompt-arrow">&gt;</span> ' +
        '<span class="error-404__log-text">' +
        escapeHtml(trimmed) +
        "</span>",
    );

    var parts = trimmed.split(/\s+/);
    var cmd = parts.shift().toLowerCase();
    var fn = commands[cmd];

    if (fn) {
      try {
        fn(parts);
      } catch (err) {
        appendText(
          "internal error: " + (err && err.message ? err.message : err),
          "error-404__log-err",
        );
      }
    } else {
      // Bare name: try fuzzy-opening a page
      var hit = fuzzyFindRoute(trimmed);
      if (hit) {
        appendText(
          cmd +
            ": not a command — opening closest match " +
            hit.url,
          "error-404__log-text",
        );
        navigate(hit.url);
      } else {
        appendText(
          cmd + ": command not found. Try 'help'.",
          "error-404__log-err",
        );
      }
    }
  }

  if (input) {
    input.addEventListener("keydown", function (e) {
      if (e.key === "Enter") {
        e.preventDefault();
        var value = input.value;
        input.value = "";
        if (value.trim()) {
          history.push(value);
          if (history.length > 50) history.shift();
        }
        historyIndex = history.length;
        run(value);
        scrollToBottom();
      } else if (e.key === "ArrowUp") {
        if (!history.length) return;
        e.preventDefault();
        historyIndex = Math.max(0, historyIndex - 1);
        input.value = history[historyIndex] || "";
      } else if (e.key === "ArrowDown") {
        if (!history.length) return;
        e.preventDefault();
        historyIndex = Math.min(history.length, historyIndex + 1);
        input.value = history[historyIndex] || "";
      }
    });

    var terminal = root.querySelector(".error-404__terminal");
    if (terminal) {
      terminal.addEventListener("click", function (e) {
        if (e.target.tagName === "A") return;
        input.focus();
      });
    }
  }
})();
