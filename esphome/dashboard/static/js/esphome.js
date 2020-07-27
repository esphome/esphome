'use strict';

// Document Ready
$(document).ready(function () {
  M.AutoInit(document.body);
  nodeGrid();
  startAceWebsocket();
  fixNavbarHeight();
});

// WebSocket URL Helper
const loc = window.location;
const wsLoc = new URL("./", `${loc.protocol}//${loc.host}${loc.pathname}`);
wsLoc.protocol = 'ws:';
if (loc.protocol === "https:") {
  wsLoc.protocol = 'wss:';
}
const wsUrl = wsLoc.href;

/**
 * Fix NavBar height
 */
const fixNavbarHeight = () => {
  const fixFunc = () => {
    const sel = $(".select-wrapper");
    $(".navbar-fixed").css("height", (sel.position().top + sel.outerHeight()) + "px");
  }
  $(window).resize(fixFunc);
  fixFunc();
}

/**
 *  Dashboard Dynamic Grid
 */
const nodeGrid = () => {
  const nodeCount = document.querySelectorAll("#nodes .card").length;
  const nodeGrid = document.querySelector("#nodes #grid");

  if (nodeCount <= 3) {
    nodeGrid.classList.add("grid-1-col");
  } else if (nodeCount <= 6) {
    nodeGrid.classList.add("grid-2-col");
  } else {
    nodeGrid.classList.add("grid-3-col");
  }
}

/**
 *  Online/ Offline Status Indication
 */

let isFetchingPing = false;

const fetchPing = () => {
  if (isFetchingPing) {
    return;
  }

  isFetchingPing = true;

  fetch(`./ping`, { credentials: "same-origin" }).then(res => res.json())
    .then(response => {
      for (let filename in response) {
        let node = document.querySelector(`#nodes .card[data-filename="${filename}"]`);

        if (node === null) {
          continue;
        }

        let status = response[filename];
        let className;

        if (status === null) {
          className = 'status-unknown';
        } else if (status === true) {
          className = 'status-online';
          node.setAttribute('data-last-connected', Date.now().toString());
        } else if (node.hasAttribute('data-last-connected')) {
          const attr = parseInt(node.getAttribute('data-last-connected'));
          if (Date.now() - attr <= 5000) {
            className = 'status-not-responding';
          } else {
            className = 'status-offline';
          }
        } else {
          className = 'status-offline';
        }

        if (node.classList.contains(className)) {
          continue;
        }

        node.classList.remove('status-unknown', 'status-online', 'status-offline', 'status-not-responding');
        node.classList.add(className);
      }

      isFetchingPing = false;
    });
};
setInterval(fetchPing, 2000);
fetchPing();

/**
 *  Log Color Parsing
 */

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
  if (pre.scrollTop + 56 >= (pre.scrollHeight - pre.offsetHeight)) {
    // at bottom
    pre.scrollTop = pre.scrollHeight;
  }
};

/**
 *  Serial Port Selection
 */

const portSelect = document.querySelector('.nav-wrapper select');
let ports = [];

const fetchSerialPorts = (begin = false) => {
  fetch(`./serial-ports`, { credentials: "same-origin" }).then(res => res.json())
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
        M.toast({ html: "Discovered new serial port." });
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

/**
 * Log Elements
 */

// Log Modal Class
class LogModal {
  constructor({
    name,
    onPrepare = (modalElement, config) => { },
    onProcessExit = (modalElement, code) => { },
    onSocketClose = (modalElement) => { },
    dismissible = true
  }) {
    this.modalId = `js-${name}-modal`;
    this.dataAction = `${name}`;
    this.wsUrl = `${wsUrl}${name}`;
    this.dismissible = dismissible;
    this.activeFilename = null;

    this.modalElement = document.getElementById(this.modalId);
    this.nodeFilenameElement = document.querySelector(`#${this.modalId} #js-node-filename`);
    this.logElement = document.querySelector(`#${this.modalId} #js-log-area`);
    this.onPrepare = onPrepare;
    this.onProcessExit = onProcessExit;
    this.onSocketClose = onSocketClose;
  }

  setup() {
    const boundOnPress = this._onPress.bind(this);
    document.querySelectorAll(`[data-action="${this.dataAction}"]`).forEach((button) => {
      button.addEventListener('click', boundOnPress);
    });
  }

  _setupModalInstance() {
    this.modalInstance = M.Modal.init(this.modalElement, {
      onOpenStart: this._onOpenStart.bind(this),
      onCloseStart: this._onCloseStart.bind(this),
      dismissible: this.dismissible
    })
  }

  _onOpenStart() {
    document.addEventListener('keydown', this._boundKeydown);
  }

  _onCloseStart() {
    document.removeEventListener('keydown', this._boundKeydown);
    this.activeSocket.close();
  }

  open(event) {
    this._onPress(event);
  }

  _onPress(event) {
    this.activeFilename = event.target.getAttribute('data-filename');

    this._setupModalInstance();
    this.nodeFilenameElement.innerHTML = this.activeFilename;

    this.logElement.innerHTML = "";
    const colorLogState = initializeColorState();

    this.onPrepare(this.modalElement, this.activeFilename);

    let stopped = false;

    this.modalInstance.open();

    const socket = new WebSocket(this.wsUrl);
    this.activeSocket = socket;
    socket.addEventListener('message', (event) => {
      const data = JSON.parse(event.data);
      if (data.event === "line") {
        colorReplace(this.logElement, colorLogState, data.data);
      } else if (data.event === "exit") {
        this.onProcessExit(this.modalElement, data.code);
        stopped = true;
      }
    });

    socket.addEventListener('open', () => {
      const msg = JSON.stringify(this._encodeSpawnMessage(this.activeFilename));
      socket.send(msg);
    });

    socket.addEventListener('close', () => {
      if (!stopped) {
        this.onSocketClose(this.modalElement);
      }
    });
  }

  _onKeyDown(event) {
    // Close on escape key
    if (event.keyCode === 27) {
      this.modalInstance.close();
    }
  }

  _encodeSpawnMessage(filename) {
    return {
      type: 'spawn',
      configuration: filename,
      port: getUploadPort(),
    };
  }
}

// Logs Modal
const logsModal = new LogModal({
  name: "logs",

  onPrepare: (modalElement, activeFilename) => {
    modalElement.querySelector("[data-action='stop-logs']").innerHTML = "Stop";
  },

  onProcessExit: (modalElement, code) => {
    if (code === 0) {
      M.toast({
        html: "Program exited successfully",
        displayLength: 10000
      });
    } else {
      M.toast({
        html: `Program failed with code ${code}`,
        displayLength: 10000
      });
    }
    modalElem.querySelector("data-action='stop-logs'").innerHTML = "Close";
  },

  onSocketClose: (modalElement) => {
    M.toast({
      html: 'Terminated process',
      displayLength: 10000
    });
  }
})

logsModal.setup();

// Upload Modal
const uploadModal = new LogModal({
  name: "upload",
  onPrepare: (modalElement, activeFilename) => {
    modalElement.querySelector("#js-upload-modal [data-action='download-binary']").classList.add('disabled');
    modalElement.querySelector("#js-upload-modal [data-action='upload']").setAttribute('data-filename', activeFilename);
    modalElement.querySelector("#js-upload-modal [data-action='upload']").classList.add('disabled');
    modalElement.querySelector("#js-upload-modal [data-action='edit']").setAttribute('data-filename', activeFilename);
    modalElement.querySelector("#js-upload-modal [data-action='stop-logs']").innerHTML = "Stop";
  },

  onProcessExit: (modalElement, code) => {
    if (code === 0) {
      M.toast({
        html: "Program exited successfully",
        displayLength: 10000
      });

      modalElement.querySelector("#js-upload-modal [data-action='download-binary']").classList.remove('disabled');
    } else {
      M.toast({
        html: `Program failed with code ${code}`,
        displayLength: 10000
      });

      modalElement.querySelector("#js-upload-modal [data-action='upload']").classList.add('disabled');
      modalElement.querySelector("#js-upload-modal [data-action='upload']").classList.remove('disabled');
    }
    modalElement.querySelector("#js-upload-modal [data-action='stop-logs']").innerHTML = "Close";
  },

  onSocketClose: (modalElement) => {
    M.toast({
      html: 'Terminated process',
      displayLength: 10000
    });
  },

  dismissible: false
})

uploadModal.setup();

const downloadAfterUploadButton = document.querySelector("#js-upload-modal [data-action='download-binary']");
downloadAfterUploadButton.addEventListener('click', () => {
  const link = document.createElement("a");
  link.download = name;
  link.href = `./download.bin?configuration=${encodeURIComponent(uploadModal.activeFilename)}`;
  document.body.appendChild(link);
  link.click();
  link.remove();
});

// Validate Modal
const validateModal = new LogModal({
  name: 'validate',
  onPrepare: (modalElement, activeFilename) => {
    modalElement.querySelector("#js-validate-modal [data-action='stop-logs']").innerHTML = "Stop";
    modalElement.querySelector("#js-validate-modal [data-action='edit']").setAttribute('data-filename', activeFilename);
    modalElement.querySelector("#js-validate-modal [data-action='upload']").setAttribute('data-filename', activeFilename);
    modalElement.querySelector("#js-validate-modal [data-action='upload']").classList.add('disabled');
  },
  onProcessExit: (modalElement, code) => {
    if (code === 0) {
      M.toast({
        html: `<code class="inlinecode">${validateModal.activeFilename}</code> is valid 👍`,
        displayLength: 10000,
      });
      modalElement.querySelector("#js-validate-modal [data-action='upload']").classList.remove('disabled');
    } else {
      M.toast({
        html: `<code class="inlinecode">${validateModal.activeFilename}</code> is invalid 😕`,
        displayLength: 10000,
      });
    }
    modalElement.querySelector("#js-validate-modal [data-action='stop-logs']").innerHTML = "Close";
  },
  onSocketClose: (modalElement) => {
    M.toast({
      html: 'Terminated process',
      displayLength: 10000,
    });
  },
});

validateModal.setup();

// Compile Modal
const compileModal = new LogModal({
  name: 'compile',
  onPrepare: (modalElement, activeFilename) => {
    modalElement.querySelector("#js-compile-modal [data-action='stop-logs']").innerHTML = "Stop";
    modalElement.querySelector("#js-compile-modal [data-action='download-binary']").classList.add('disabled');
  },
  onProcessExit: (modalElement, code) => {
    if (code === 0) {
      M.toast({
        html: "Program exited successfully",
        displayLength: 10000,
      });
      modalElement.querySelector("#js-compile-modal [data-action='download-binary']").classList.remove('disabled');
    } else {
      M.toast({
        html: `Program failed with code ${data.code}`,
        displayLength: 10000,
      });
    }
    modalElement.querySelector("#js-compile-modal [data-action='stop-logs']").innerHTML = "Close";
  },
  onSocketClose: (modalElement) => {
    M.toast({
      html: 'Terminated process',
      displayLength: 10000,
    });
  },
  dismissible: false,
});

compileModal.setup();

const downloadAfterCompileButton = document.querySelector("#js-compile-modal [data-action='download-binary']");
downloadAfterCompileButton.addEventListener('click', () => {
  const link = document.createElement("a");
  link.download = name;
  link.href = `./download.bin?configuration=${encodeURIComponent(compileModal.activeFilename)}`;
  document.body.appendChild(link);
  link.click();
  link.remove();
});

// Clean MQTT Modal
const cleanMqttModal = new LogModal({
  name: 'clean-mqtt',
  onPrepare: (modalElement, activeFilename) => {
    modalElement.querySelector("#js-clean-mqtt-modal [data-action='stop-logs']").innerHTML = "Stop";
  },
  onProcessExit: (modalElement, code) => {
    if (code === 0) {
      M.toast({
        html: "Program exited successfully",
        displayLength: 10000,
      });
    } else {
      M.toast({
        html: `Program failed with code ${code}`,
        displayLength: 10000,
      });
    }
    modalElement.querySelector("#js-clean-mqtt-modal [data-action='stop-logs']").innerHTML = "Close";
  },
  onSocketClose: (modalElement) => {
    M.toast({
      html: 'Terminated process',
      displayLength: 10000,
    });
  },
});

cleanMqttModal.setup();

// Clean Build Files Modal
const cleanModal = new LogModal({
  name: 'clean',
  onPrepare: (modalElement, activeFilename) => {
    modalElement.querySelector("#js-clean-modal [data-action='stop-logs']").innerHTML = "Stop";
  },
  onProcessExit: (modalElement, code) => {
    if (code === 0) {
      M.toast({
        html: "Program exited successfully",
        displayLength: 10000,
      });
    } else {
      M.toast({
        html: `Program failed with code ${code}`,
        displayLength: 10000,
      });
    }
    modalElement.querySelector("#js-clean-modal [data-action='stop-logs']").innerHTML = "Close";
  },
  onSocketClose: (modalElement) => {
    M.toast({
      html: 'Terminated process',
      displayLength: 10000,
    });
  },
});

cleanModal.setup();

// Update All Modal
const updateAllModal = new LogModal({
  name: 'update-all',
  onPrepare: (modalElement, activeFilename) => {
    modalElement.querySelector("#js-update-all-modal [data-action='stop-logs']").innerHTML = "Stop";
    modalElement.querySelector("#js-update-all-modal #js-node-filename").style.visibility = "hidden";
  },
  onProcessExit: (modalElement, code) => {
    if (code === 0) {
      M.toast({
        html: "Program exited successfully",
        displayLength: 10000,
      });
      downloadButton.classList.remove('disabled');
    } else {
      M.toast({
        html: `Program failed with code ${data.code}`,
        displayLength: 10000,
      });
    }
    modalElement.querySelector("#js-update-all-modal [data-action='stop-logs']").innerHTML = "Close";
  },
  onSocketClose: (modalElement) => {
    M.toast({
      html: 'Terminated process',
      displayLength: 10000,
    });
  },
  dismissible: false,
});

updateAllModal.setup();

/**
 *  Node Editing
 */

let editorActiveFilename = null;
let editorActiveSecrets = false;
let editorActiveWebSocket = null;
let editorValidationScheduled = false;
let editorValidationRunning = false;

// Setup Editor
const editorElement = document.querySelector("#js-editor-modal #js-editor-area");
const editor = ace.edit(editorElement);

editor.setOptions({
  highlightActiveLine: true,
  showPrintMargin: true,
  useSoftTabs: true,
  tabSize: 2,
  useWorker: false,
  theme: 'ace/theme/dreamweaver',
  mode: 'ace/mode/yaml'
});

editor.commands.addCommand({
  name: 'saveCommand',
  bindKey: { win: 'Ctrl-S', mac: 'Command-S' },
  exec: function () {
    saveFile(editorActiveFilename);
  },
  readOnly: false
});

// Edit Button Listener
document.querySelectorAll("[data-action='edit']").forEach((button) => {
  button.addEventListener('click', (event) => {

    editorActiveFilename = event.target.getAttribute("data-filename");
    const filenameField = document.querySelector("#js-editor-modal #js-node-filename");
    filenameField.innerHTML = editorActiveFilename;

    const saveButton = document.querySelector("#js-editor-modal [data-action='save']");
    const uploadButton = document.querySelector("#js-editor-modal [data-action='upload']");
    const closeButton = document.querySelector("#js-editor-modal [data-action='close']");
    saveButton.setAttribute('data-filename', editorActiveFilename);
    uploadButton.setAttribute('data-filename', editorActiveFilename);
    uploadButton.setAttribute('onClick', `saveFile("${editorActiveFilename}")`);
    if (editorActiveFilename === "secrets.yaml") {
      uploadButton.classList.add("disabled");
      editorActiveSecrets = true;
    } else {
      uploadButton.classList.remove("disabled");
      editorActiveSecrets = false;
    }
    closeButton.setAttribute('data-filename', editorActiveFilename);

    const loadingIndicator = document.querySelector("#js-editor-modal #js-loading-indicator");
    const editorArea = document.querySelector("#js-editor-modal #js-editor-area");

    loadingIndicator.style.display = "block";
    editorArea.style.display = "none";

    editor.setOption('readOnly', true);
    fetch(`./edit?configuration=${editorActiveFilename}`, { credentials: "same-origin" })
      .then(res => res.text()).then(response => {
        editor.setValue(response, -1);
        editor.setOption('readOnly', false);
        loadingIndicator.style.display = "none";
        editorArea.style.display = "block";
      });
    editor.focus();

    const editModalElement = document.getElementById("js-editor-modal");
    const editorModal = M.Modal.init(editModalElement, {
      onOpenStart: function () {
        editorModalOnOpen()
      },
      onCloseStart: function () {
        editorModalOnClose()
      },
      dismissible: false
    })

    editorModal.open();

  });
});

// Editor On Open
const editorModalOnOpen = () => {
  return
}

// Editor On Close
const editorModalOnClose = () => {
  editorActiveFilename = null;
}

// Editor WebSocket Validation
const startAceWebsocket = () => {
  editorActiveWebSocket = new WebSocket(`${wsUrl}ace`);

  editorActiveWebSocket.addEventListener('message', (event) => {
    const raw = JSON.parse(event.data);
    if (raw.event === "line") {
      const msg = JSON.parse(raw.data);
      if (msg.type === "result") {
        const arr = [];

        for (const v of msg.validation_errors) {
          let o = {
            text: v.message,
            type: 'error',
            row: 0,
            column: 0
          };
          if (v.range != null) {
            o.row = v.range.start_line;
            o.column = v.range.start_col;
          }
          arr.push(o);
        }
        for (const v of msg.yaml_errors) {
          arr.push({
            text: v.message,
            type: 'error',
            row: 0,
            column: 0
          });
        }

        editor.session.setAnnotations(arr);

        if (arr.length) {
          document.querySelector("#js-editor-modal [data-action='upload']").classList.add('disabled');
        } else {
          document.querySelector("#js-editor-modal [data-action='upload']").classList.remove('disabled');
        }

        editorValidationRunning = false;
      } else if (msg.type === "read_file") {
        sendAceStdin({
          type: 'file_response',
          content: editor.getValue()
        });
      }
    }
  })

  editorActiveWebSocket.addEventListener('open', () => {
    const msg = JSON.stringify({ type: 'spawn' });
    editorActiveWebSocket.send(msg);
  });

  editorActiveWebSocket.addEventListener('close', () => {
    editorActiveWebSocket = null;
    setTimeout(startAceWebsocket, 5000);
  });
};

const sendAceStdin = (data) => {
  let send = JSON.stringify({
    type: 'stdin',
    data: JSON.stringify(data) + '\n',
  });
  editorActiveWebSocket.send(send);
};

const debounce = (func, wait) => {
  let timeout;
  return function () {
    let context = this, args = arguments;
    let later = function () {
      timeout = null;
      func.apply(context, args);
    };
    clearTimeout(timeout);
    timeout = setTimeout(later, wait);
  };
};

editor.session.on('change', debounce(() => {
  editorValidationScheduled = !editorActiveSecrets;
}, 250));

setInterval(() => {
  if (!editorValidationScheduled || editorValidationRunning)
    return;
  if (editorActiveWebSocket == null)
    return;

  sendAceStdin({
    type: 'validate',
    file: editorActiveFilename
  });
  editorValidationRunning = true;
  editorValidationScheduled = false;
}, 100);

// Save File
const saveFile = (filename) => {
  const extensionRegex = new RegExp("(?:\.([^.]+))?$");

  if (filename.match(extensionRegex)[0] !== ".yaml") {
    M.toast({
      html: `❌ File <code class="inlinecode">${filename}</code> cannot be saved as it is not a YAML file!`,
      displayLength: 10000
    });
    return;
  }

  fetch(`./edit?configuration=${filename}`, {
    credentials: "same-origin",
    method: "POST",
    body: editor.getValue()
  })
    .then((response) => {
      response.text();
    })
    .then(() => {
      M.toast({
        html: `✅ Saved <code class="inlinecode">${filename}</code>`,
        displayLength: 10000
      });
    })
    .catch((error) => {
      M.toast({
        html: `❌ An error occured saving <code class="inlinecode">${filename}</code>`,
        displayLength: 10000
      });
    })
}

document.querySelectorAll("[data-action='save']").forEach((btn) => {
  btn.addEventListener("click", (e) => {
    saveFile(editorActiveFilename);
  });
});

// Delete Node
document.querySelectorAll("[data-action='delete']").forEach((btn) => {
  btn.addEventListener("click", (e) => {
    const filename = e.target.getAttribute("data-filename");

    fetch(`./delete?configuration=${filename}`, {
      credentials: "same-origin",
      method: "POST",
    })
      .then((res) => {
        res.text()
      })
      .then(() => {
        const toastHtml = `<span>🗑️ Deleted <code class="inlinecode">${filename}</code>
                           <button class="btn-flat toast-action">Undo</button>`;
        const toast = M.toast({
          html: toastHtml,
          displayLength: 10000
        });
        const undoButton = toast.el.querySelector('.toast-action');

        document.querySelector(`.card[data-filename="${filename}"]`).remove();

        undoButton.addEventListener('click', () => {
          fetch(`./undo-delete?configuration=${filename}`, {
            credentials: "same-origin",
            method: "POST",
          })
            .then((res) => {
              res.text()
            })
            .then(() => {
              window.location.reload(false);
            });
        });
      });

  });
});

/**
 *  Wizard
 */

const wizardTriggerElement = document.querySelector("[data-action='wizard']");
const wizardModal = document.getElementById("js-wizard-modal");
const wizardCloseButton = document.querySelector("[data-action='wizard-close']");

const wizardStepper = document.querySelector('#js-wizard-modal .stepper');
const wizardStepperInstace = new MStepper(wizardStepper, {
  firstActive: 0,
  stepTitleNavigation: false,
  autoFocusInput: true,
  showFeedbackLoader: true,
})

const startWizard = () => {
  M.Modal.init(wizardModal, {
    dismissible: false
  }).open();
}

document.querySelectorAll("[data-action='wizard']").forEach((btn) => {
  btn.addEventListener("click", (event) => {
    startWizard();
  })
});

jQuery.validator.addMethod("nospaces", (value, element) => {
  return value.indexOf(' ') < 0;
}, "Name cannot contain any spaces!");

jQuery.validator.addMethod("lowercase", (value, element) => {
  return value === value.toLowerCase();
}, "Name must be all lower case!");
