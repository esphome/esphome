// Document Ready
$(document).ready(function () {
  nodeGrid();
});

// Add grid class depedant upon number of nodes on display
const nodeGrid = () => {
  const nodeCount = $('#nodes .card').length
  const nodeGrid = $('#nodes #grid')

  if (nodeCount >= 6) {
    $(nodeGrid).addClass("grid-2-col");
  } else {
    $(nodeGrid).addClass("grid-1-col");
  }
}

// Online/Offline Status Indicator
let isFetchingPing = false;

const fetchPing = () => {
  if (isFetchingPing) {
    return;
  }

  isFetchingPing = true;

  fetch(`./ping`, { credentials: "same-origin" }).then(res => res.json())
    .then(response => {
      for (let filename in response) {
        let node = document.querySelector(`#nodes .card[data-node="${filename}"]`);

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