document.addEventListener('DOMContentLoaded', () => {
  M.AutoInit(document.body);
});

const initializeColorState = () => {
  return {
    bold: false,
    italic: false,
    underline: false,
    strikethrough: false,
    foregroundColor: false,
    backgroundColor: false,
    carriageReturn: false,
    secret: false,
  };
};

const colorReplace = (pre, state, text) => {
  const re = /(?:\033|\\033)(?:\[(.*?)[@-~]|\].*?(?:\007|\033\\))/g;
  let i = 0;

  if (state.carriageReturn) {
    if (text !== "\n") {
      // don't remove if \r\n
      pre.removeChild(pre.lastChild);
    }
    state.carriageReturn = false;
  }

  if (text.includes("\r")) {
    state.carriageReturn = true;
  }

  const lineSpan = document.createElement("span");
  lineSpan.classList.add("line");
  pre.appendChild(lineSpan);

  const addSpan = (content) => {
    if (content === "")
      return;

    const span = document.createElement("span");
    if (state.bold) span.classList.add("log-bold");
    if (state.italic) span.classList.add("log-italic");
    if (state.underline) span.classList.add("log-underline");
    if (state.strikethrough) span.classList.add("log-strikethrough");
    if (state.secret) span.classList.add("log-secret");
    if (state.foregroundColor !== null) span.classList.add(`log-fg-${state.foregroundColor}`);
    if (state.backgroundColor !== null) span.classList.add(`log-bg-${state.backgroundColor}`);
    span.appendChild(document.createTextNode(content));
    lineSpan.appendChild(span);

    if (state.secret) {
      const redacted = document.createElement("span");
      redacted.classList.add("log-secret-redacted");
      redacted.appendChild(document.createTextNode("[redacted]"));
      lineSpan.appendChild(redacted);
    }
  };


  while (true) {
    const match = re.exec(text);
    if (match === null)
      break;

    const j = match.index;
    addSpan(text.substring(i, j));
    i = j + match[0].length;

    if (match[1] === undefined) continue;

    for (const colorCode of match[1].split(";")) {
      switch (parseInt(colorCode)) {
        case 0:
          // reset
          state.bold = false;
          state.italic = false;
          state.underline = false;
          state.strikethrough = false;
          state.foregroundColor = null;
          state.backgroundColor = null;
          state.secret = false;
          break;
        case 1:
          state.bold = true;
          break;
        case 3:
          state.italic = true;
          break;
        case 4:
          state.underline = true;
          break;
        case 5:
          state.secret = true;
          break;
        case 6:
          state.secret = false;
          break;
        case 9:
          state.strikethrough = true;
          break;
        case 22:
          state.bold = false;
          break;
        case 23:
          state.italic = false;
          break;
        case 24:
          state.underline = false;
          break;
        case 29:
          state.strikethrough = false;
          break;
        case 30:
          state.foregroundColor = "black";
          break;
        case 31:
          state.foregroundColor = "red";
          break;
        case 32:
          state.foregroundColor = "green";
          break;
        case 33:
          state.foregroundColor = "yellow";
          break;
        case 34:
          state.foregroundColor = "blue";
          break;
        case 35:
          state.foregroundColor = "magenta";
          break;
        case 36:
          state.foregroundColor = "cyan";
          break;
        case 37:
          state.foregroundColor = "white";
          break;
        case 39:
          state.foregroundColor = null;
          break;
        case 41:
          state.backgroundColor = "red";
          break;
        case 42:
          state.backgroundColor = "green";
          break;
        case 43:
          state.backgroundColor = "yellow";
          break;
        case 44:
          state.backgroundColor = "blue";
          break;
        case 45:
          state.backgroundColor = "magenta";
          break;
        case 46:
          state.backgroundColor = "cyan";
          break;
        case 47:
          state.backgroundColor = "white";
          break;
        case 40:
        case 49:
          state.backgroundColor = null;
          break;
      }
    }
  }
  addSpan(text.substring(i));
  scrollToBottomOfElement(pre);
};

const removeUpdateAvailable = (filename) => {
  const p = document.querySelector(`.update-available[data-node="${filename}"]`);
  if (p === undefined)
    return;
  p.remove();
};

let configuration = "";
let wsProtocol = "ws:";
if (window.location.protocol === "https:") {
  wsProtocol = 'wss:';
}
const wsUrl = wsProtocol + '//' + window.location.hostname + ':' + window.location.port;

let isFetchingPing = false;
const fetchPing = () => {
  if (isFetchingPing)
      return;
  isFetchingPing = true;

  fetch('/ping', {credentials: "same-origin"}).then(res => res.json())
    .then(response => {
      for (let filename in response) {
        let node = document.querySelector(`.status-indicator[data-node="${filename}"]`);
        if (node === null)
          continue;

        let status = response[filename];
        let klass;
        if (status === null) {
          klass = 'unknown';
        } else if (status === true) {
          klass = 'online';
          node.setAttribute('data-last-connected', Date.now().toString());
        } else if (node.hasAttribute('data-last-connected')) {
          const attr = parseInt(node.getAttribute('data-last-connected'));
          if (Date.now() - attr <= 5000) {
            klass = 'not-responding';
          } else {
            klass = 'offline';
          }
        } else {
          klass = 'offline';
        }

        if (node.classList.contains(klass))
          continue;

        node.classList.remove('unknown', 'online', 'offline', 'not-responding');
        node.classList.add(klass);
      }

      isFetchingPing = false;
    });
};
setInterval(fetchPing, 2000);
fetchPing();

const portSelect = document.querySelector('.nav-wrapper select');
let ports = [];

const fetchSerialPorts = (begin=false) => {
  fetch('/serial-ports', {credentials: "same-origin"}).then(res => res.json())
    .then(response => {
      if (ports.length === response.length) {
        let allEqual = true;
        for (let i = 0; i < response.length; i++) {
          if (ports[i].port !== response[i].port) {
            allEqual = false;
            break;
          }
        }
        if (allEqual)
          return;
      }
      const hasNewPort = response.length >= ports.length;

      ports = response;

      const inst = M.FormSelect.getInstance(portSelect);
      if (inst !== undefined) {
        inst.destroy();
      }

      portSelect.innerHTML = "";
      const prevSelected = getUploadPort();
      for (let i = 0; i < response.length; i++) {
        const val = response[i];
        if (val.port === prevSelected) {
          portSelect.innerHTML += `<option value="${val.port}" selected>${val.port} (${val.desc})</option>`;
        } else {
          portSelect.innerHTML += `<option value="${val.port}">${val.port} (${val.desc})</option>`;
        }
      }

      M.FormSelect.init(portSelect, {});
      if (!begin && hasNewPort)
        M.toast({html: "Discovered new serial port."});
    });
};

const getUploadPort = () => {
  const inst = M.FormSelect.getInstance(portSelect);
  if (inst === undefined) {
    return "OTA";
  }

  inst._setSelectedStates();
  return inst.getSelectedValues()[0];
};
setInterval(fetchSerialPorts, 5000);
fetchSerialPorts(true);

const logsModalElem = document.getElementById("modal-logs");

document.querySelectorAll(".action-show-logs").forEach((showLogs) => {
  showLogs.addEventListener('click', (e) => {
    configuration = e.target.getAttribute('data-node');
    const modalInstance = M.Modal.getInstance(logsModalElem);
    const log = logsModalElem.querySelector(".log");
    log.innerHTML = "";
    const colorState = initializeColorState();
    const stopLogsButton = logsModalElem.querySelector(".stop-logs");
    let stopped = false;
    stopLogsButton.innerHTML = "Stop";
    modalInstance.open();

    const filenameField = logsModalElem.querySelector('.filename');
    filenameField.innerHTML = configuration;

    const logSocket = new WebSocket(wsUrl + "/logs");
    logSocket.addEventListener('message', (event) => {
      const data = JSON.parse(event.data);
      if (data.event === "line") {
        colorReplace(log, colorState, data.data);
      } else if (data.event === "exit") {
        if (data.code === 0) {
          M.toast({html: "Program exited successfully."});
        } else {
          M.toast({html: `Program failed with code ${data.code}`});
        }

        stopLogsButton.innerHTML = "Close";
        stopped = true;
      }
    });
    logSocket.addEventListener('open', () => {
      const msg = JSON.stringify({configuration: configuration, port: getUploadPort()});
      logSocket.send(msg);
    });
    logSocket.addEventListener('close', () => {
      if (!stopped) {
        M.toast({html: 'Terminated process.'});
      }
    });
    modalInstance.options.onCloseStart = () => {
      logSocket.close();
    };
  });
});

const uploadModalElem = document.getElementById("modal-upload");

document.querySelectorAll(".action-upload").forEach((upload) => {
  upload.addEventListener('click', (e) => {
    configuration = e.target.getAttribute('data-node');
    const modalInstance = M.Modal.getInstance(uploadModalElem);
    modalInstance.options['dismissible'] = false;
    const log = uploadModalElem.querySelector(".log");
    log.innerHTML = "";
    const colorState = initializeColorState();
    const stopLogsButton = uploadModalElem.querySelector(".stop-logs");
    let stopped = false;
    stopLogsButton.innerHTML = "Stop";
    modalInstance.open();

    const filenameField = uploadModalElem.querySelector('.filename');
    filenameField.innerHTML = configuration;

    const logSocket = new WebSocket(wsUrl + "/run");
    logSocket.addEventListener('message', (event) => {
      const data = JSON.parse(event.data);
      if (data.event === "line") {
        colorReplace(log, colorState, data.data);
      } else if (data.event === "exit") {
        if (data.code === 0) {
          M.toast({html: "Program exited successfully."});
          removeUpdateAvailable(configuration);
        } else {
          M.toast({html: `Program failed with code ${data.code}`});
        }

        stopLogsButton.innerHTML = "Close";
        stopped = true;
      }
    });
    logSocket.addEventListener('open', () => {
      const msg = JSON.stringify({configuration: configuration, port: getUploadPort()});
      logSocket.send(msg);
    });
    logSocket.addEventListener('close', () => {
      if (!stopped) {
        M.toast({html: 'Terminated process.'});
      }
    });
    modalInstance.options.onCloseStart = () => {
      logSocket.close();
    };
  });
});

const validateModalElem = document.getElementById("modal-validate");

document.querySelectorAll(".action-validate").forEach((upload) => {
  upload.addEventListener('click', (e) => {
    configuration = e.target.getAttribute('data-node');
    const modalInstance = M.Modal.getInstance(validateModalElem);
    const log = validateModalElem.querySelector(".log");
    log.innerHTML = "";
    const colorState = initializeColorState();
    const stopLogsButton = validateModalElem.querySelector(".stop-logs");
    let stopped = false;
    stopLogsButton.innerHTML = "Stop";
    modalInstance.open();

    const filenameField = validateModalElem.querySelector('.filename');
    filenameField.innerHTML = configuration;

    const logSocket = new WebSocket(wsUrl + "/validate");
    logSocket.addEventListener('message', (event) => {
      const data = JSON.parse(event.data);
      if (data.event === "line") {
        colorReplace(log, colorState, data.data);
      } else if (data.event === "exit") {
        if (data.code === 0) {
          M.toast({
            html: `<code class="inlinecode">${configuration}</code> is valid 👍`,
            displayLength: 5000,
          });
        } else {
          M.toast({
            html: `<code class="inlinecode">${configuration}</code> is invalid 😕`,
            displayLength: 5000,
          });
        }

        stopLogsButton.innerHTML = "Close";
        stopped = true;
      }
    });
    logSocket.addEventListener('open', () => {
      const msg = JSON.stringify({configuration: configuration});
      logSocket.send(msg);
    });
    logSocket.addEventListener('close', () => {
      if (!stopped) {
        M.toast({html: 'Terminated process.'});
      }
    });
    modalInstance.options.onCloseStart = () => {
      logSocket.close();
    };
  });
});

const compileModalElem = document.getElementById("modal-compile");
const downloadButton = compileModalElem.querySelector('.download-binary');

document.querySelectorAll(".action-compile").forEach((upload) => {
  upload.addEventListener('click', (e) => {
    configuration = e.target.getAttribute('data-node');
    const modalInstance = M.Modal.getInstance(compileModalElem);
    modalInstance.options['dismissible'] = false;
    const log = compileModalElem.querySelector(".log");
    log.innerHTML = "";
    const colorState = initializeColorState();
    const stopLogsButton = compileModalElem.querySelector(".stop-logs");
    let stopped = false;
    stopLogsButton.innerHTML = "Stop";
    downloadButton.classList.add('disabled');

    modalInstance.open();

    const filenameField = compileModalElem.querySelector('.filename');
    filenameField.innerHTML = configuration;

    const logSocket = new WebSocket(wsUrl + "/compile");
    logSocket.addEventListener('message', (event) => {
      const data = JSON.parse(event.data);
      if (data.event === "line") {
        colorReplace(log, colorState, data.data);
      } else if (data.event === "exit") {
        if (data.code === 0) {
          M.toast({html: "Program exited successfully."});
          downloadButton.classList.remove('disabled');
        } else {
          M.toast({html: `Program failed with code ${data.code}`});
        }

        stopLogsButton.innerHTML = "Close";
        stopped = true;
      }
    });
    logSocket.addEventListener('open', () => {
      const msg = JSON.stringify({configuration: configuration});
      logSocket.send(msg);
    });
    logSocket.addEventListener('close', () => {
      if (!stopped) {
        M.toast({html: 'Terminated process.'});
      }
    });
    modalInstance.options.onCloseStart = () => {
      logSocket.close();
    };
  });
});

downloadButton.addEventListener('click', () => {
  const link = document.createElement("a");
  link.download = name;
  link.href = '/download.bin?configuration=' + encodeURIComponent(configuration);
  document.body.appendChild(link);
  link.click();
  link.remove();
});

const cleanMqttModalElem = document.getElementById("modal-clean-mqtt");

document.querySelectorAll(".action-clean-mqtt").forEach((btn) => {
  btn.addEventListener('click', (e) => {
    configuration = e.target.getAttribute('data-node');
    const modalInstance = M.Modal.getInstance(cleanMqttModalElem);
    const log = cleanMqttModalElem.querySelector(".log");
    log.innerHTML = "";
    const colorState = initializeColorState();
    const stopLogsButton = cleanMqttModalElem.querySelector(".stop-logs");
    let stopped = false;
    stopLogsButton.innerHTML = "Stop";
    modalInstance.open();

    const filenameField = cleanMqttModalElem.querySelector('.filename');
    filenameField.innerHTML = configuration;

    const logSocket = new WebSocket(wsUrl + "/clean-mqtt");
    logSocket.addEventListener('message', (event) => {
      const data = JSON.parse(event.data);
      if (data.event === "line") {
        colorReplace(log, colorState, data.data);
      } else if (data.event === "exit") {
        stopLogsButton.innerHTML = "Close";
        stopped = true;
      }
    });
    logSocket.addEventListener('open', () => {
      const msg = JSON.stringify({configuration: configuration});
      logSocket.send(msg);
    });
    logSocket.addEventListener('close', () => {
      if (!stopped) {
        M.toast({html: 'Terminated process.'});
      }
    });
    modalInstance.options.onCloseStart = () => {
      logSocket.close();
    };
  });
});

const cleanModalElem = document.getElementById("modal-clean");

document.querySelectorAll(".action-clean").forEach((btn) => {
  btn.addEventListener('click', (e) => {
    configuration = e.target.getAttribute('data-node');
    const modalInstance = M.Modal.getInstance(cleanModalElem);
    const log = cleanModalElem.querySelector(".log");
    log.innerHTML = "";
    const colorState = initializeColorState();
    const stopLogsButton = cleanModalElem.querySelector(".stop-logs");
    let stopped = false;
    stopLogsButton.innerHTML = "Stop";
    modalInstance.open();

    const filenameField = cleanModalElem.querySelector('.filename');
    filenameField.innerHTML = configuration;

    const logSocket = new WebSocket(wsUrl + "/clean");
    logSocket.addEventListener('message', (event) => {
      const data = JSON.parse(event.data);
      if (data.event === "line") {
        colorReplace(log, colorState, data.data);
      } else if (data.event === "exit") {
        if (data.code === 0) {
          M.toast({html: "Program exited successfully."});
          downloadButton.classList.remove('disabled');
        } else {
          M.toast({html: `Program failed with code ${data.code}`});
        }
        stopLogsButton.innerHTML = "Close";
        stopped = true;
      }
    });
    logSocket.addEventListener('open', () => {
      const msg = JSON.stringify({configuration: configuration});
      logSocket.send(msg);
    });
    logSocket.addEventListener('close', () => {
      if (!stopped) {
        M.toast({html: 'Terminated process.'});
      }
    });
    modalInstance.options.onCloseStart = () => {
      logSocket.close();
    };
  });
});

const hassConfigModalElem = document.getElementById("modal-hass-config");

document.querySelectorAll(".action-hass-config").forEach((btn) => {
  btn.addEventListener('click', (e) => {
    configuration = e.target.getAttribute('data-node');
    const modalInstance = M.Modal.getInstance(hassConfigModalElem);
    const log = hassConfigModalElem.querySelector(".log");
    log.innerHTML = "";
    const colorState = initializeColorState();
    const stopLogsButton = hassConfigModalElem.querySelector(".stop-logs");
    let stopped = false;
    stopLogsButton.innerHTML = "Stop";
    modalInstance.open();

    const filenameField = hassConfigModalElem.querySelector('.filename');
    filenameField.innerHTML = configuration;

    const logSocket = new WebSocket(wsUrl + "/hass-config");
    logSocket.addEventListener('message', (event) => {
      const data = JSON.parse(event.data);
      if (data.event === "line") {
        colorReplace(log, colorState, data.data);
      } else if (data.event === "exit") {
        if (data.code === 0) {
          M.toast({html: "Program exited successfully."});
          downloadButton.classList.remove('disabled');
        } else {
          M.toast({html: `Program failed with code ${data.code}`});
        }
        stopLogsButton.innerHTML = "Close";
        stopped = true;
      }
    });
    logSocket.addEventListener('open', () => {
      const msg = JSON.stringify({configuration: configuration});
      logSocket.send(msg);
    });
    logSocket.addEventListener('close', () => {
      if (!stopped) {
        M.toast({html: 'Terminated process.'});
      }
    });
    modalInstance.options.onCloseStart = () => {
      logSocket.close();
    };
  });
});

const editModalElem = document.getElementById("modal-editor");
const editorElem = editModalElem.querySelector("#editor");
const editor = ace.edit(editorElem);
editor.setTheme("ace/theme/dreamweaver");
editor.session.setMode("ace/mode/yaml");
editor.session.setOption('useSoftTabs', true);
editor.session.setOption('tabSize', 2);

const saveButton = editModalElem.querySelector(".save-button");
const saveEditor = () => {
  fetch(`/edit?configuration=${configuration}`, {
      credentials: "same-origin",
      method: "POST",
      body: editor.getValue()
    }).then(res => res.text()).then(() => {
      M.toast({
        html: `Saved <code class="inlinecode">${configuration}</code>`
      });
    });
};

editor.commands.addCommand({
  name: 'saveCommand',
  bindKey: {win: 'Ctrl-S',  mac: 'Command-S'},
  exec: saveEditor,
  readOnly: false
});

saveButton.addEventListener('click', saveEditor);

document.querySelectorAll(".action-edit").forEach((btn) => {
  btn.addEventListener('click', (e) => {
    configuration = e.target.getAttribute('data-node');
    const modalInstance = M.Modal.getInstance(editModalElem);
    const filenameField = editModalElem.querySelector('.filename');
    filenameField.innerHTML = configuration;

    fetch(`/edit?configuration=${configuration}`, {credentials: "same-origin"})
      .then(res => res.text()).then(response => {
        editor.setValue(response, -1);
    });

    modalInstance.open();
  });
});

const modalSetupElem = document.getElementById("modal-wizard");
const setupWizardStart = document.getElementById('setup-wizard-start');
const startWizard = () => {
  const modalInstance = M.Modal.getInstance(modalSetupElem);
  modalInstance.open();

  modalInstance.options.onCloseStart = () => {

  };

  $('.stepper').activateStepper({
    linearStepsNavigation: false,
    autoFocusInput: true,
    autoFormCreation: true,
    showFeedbackLoader: true,
    parallel: false
  });
};

const scrollToBottomOfElement = (element) => {
  var atBottom = false;
  if (element.scrollTop + 30 >= (element.scrollHeight - element.offsetHeight)) {
    atBottom = true;
  }

  if (atBottom) {
    element.scrollTop = element.scrollHeight;
  }
}

setupWizardStart.addEventListener('click', startWizard);