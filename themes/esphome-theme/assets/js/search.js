// Search functionality

document.addEventListener('DOMContentLoaded', function() {
    if (typeof PagefindModularUI === 'undefined') {
        console.error('PagefindModularUI library not loaded');
        return;
    }

    class El {
        constructor(tagname) {
            this.element = document.createElement(tagname);
        }

        id(s) {
            this.element.id = s;
            return this;
        }

        class(s) {
            this.element.classList.add(s);
            return this;
        }

        attrs(obj) {
            for (const [k, v] of Object.entries(obj)) {
                this.element.setAttribute(k, v);
            }
            return this;
        }

        text(t) {
            this.element.innerText = t;
            return this;
        }

        html(t) {
            this.element.innerHTML = t;
            return this;
        }

        handle(e, f) {
            this.element.addEventListener(e, f);
            return this;
        }

        addTo(el) {
            if (el instanceof El) {
                el.element.appendChild(this.element);
            } else {
                el.appendChild(this.element);
            }
            return this.element;
        }
    }

    function getLink(location, anchors, url) {
        if (!anchors || !anchors.length)
            return null;
        // find the closest anchor at or before the current location
        const anchor = anchors.sort((a, b) => b.location - a.location).find(a => a.location <= location);
        if (anchor) {
            return url + "#" + anchor.id;
        }
        return null;
    }

    const resultTemplate = (result) => {
        const wrapper = new El("li").class("pagefind-modular-list-result");
        wrapper.handle("click", closeResults);

        const locations = result.weighted_locations.sort((a, b) => b.weight - a.weight);
        const url = getLink(locations[0]?.location, result.anchors, result.url) || result.meta?.url || result.url;
        const thumb = new El("a").class("pagefind-modular-list-thumb").attrs({href: url}).addTo(wrapper);
        let image = result?.meta?.image;
        if (image && !image.toString().includes("images/icons/")) {
            new El("img").class("pagefind-modular-list-image").attrs({
                src: image,
                alt: result.meta.image_alt || result.meta.title
            }).addTo(thumb);
        }

        const inner = new El("div").class("pagefind-modular-list-inner").addTo(wrapper);
        const title = new El("p").class("pagefind-modular-list-title").addTo(inner);
        new El("a").class("pagefind-modular-list-link").text(result.meta?.title).attrs({
            href: result.meta?.url || result.url
        }).addTo(title);
        const excerpt =new El("a").class("pagefind-modular-list-link").attrs({
            href: url
        }).addTo(inner);
        new El("p").class("pagefind-modular-list-excerpt").html(result.excerpt).addTo(excerpt);

        return wrapper.element;
    }

// Create search input and container
    const searchContainer = document.getElementById('nav-search-container');

// Create search input
    const searchInput = document.createElement('input');
    searchInput.type = 'text';
    searchInput.id = "frontpage-search";
    searchInput.placeholder = 'Search...';
    searchInput.className = 'pagefind-ui__search-input';
    searchInput.ariaLabel = 'Search';
    searchContainer.appendChild(searchInput);

    const resultsContainer = document.getElementById('nav-search-results');

    const instance = new PagefindModularUI.Instance({
        showSubResults: true,
        showImages: false,
        resetStyles: true,
        ranking: {
            pageLength: 0.0,
            termSaturation: 1.6,
            termFrequency: 0.4,
            termSimilarity: 6.0
        }
    });

// Add input component
    instance.add(new PagefindModularUI.Input({
        inputElement: "#frontpage-search"
    }));

// Add results component
    instance.add(new PagefindModularUI.ResultList({
        containerElement: "#nav-search-results",
        resultTemplate: resultTemplate
    }));


    let top_hit = null;

    function closeResults() {
        resultsContainer.style.display = 'none';
        top_hit = null;
    }

// Show/hide results
    instance.on("results", async (results) => {
        if (results.results.length) {
            resultsContainer.style.display = 'block';
            const data = await results.results[0].data();
            top_hit = data.url;
        } else {
            closeResults();
        }
    });

    document.addEventListener('click', function (e) {
        if (!e.target.closest('#nav-search-results')) {
            closeResults();
        }
    });



// Create clear button
    const clearButton = document.createElement('button');
    clearButton.type = 'button';
    clearButton.className = 'search-clear-button';
    clearButton.textContent = "X";
    clearButton.style.display = 'none';
    searchContainer.appendChild(clearButton);

// Show/hide clear button based on input content
    searchInput.addEventListener('input', () => {
        clearButton.style.display = searchInput.value.length > 0 ? 'flex' : 'none';
    });

// Clear search when button is clicked
    clearButton.addEventListener('click', () => {
        searchInput.value = '';
        clearButton.style.display = 'none';
        instance.triggerSearch('');
        resultsContainer.style.display = 'none';
        searchInput.focus(); // Re-focus the search box after clearing
    });

    document.addEventListener('keydown', function (event) {
        if (!(searchInput === document.activeElement) && event.key === '/') { // Use '/' key as trigger
            event.preventDefault(); // Prevent the '/' key from being entered in the search box
            if (isMobileScreen())
                openMenu();
            searchInput.focus();
        }
    });
    const navContainer = document.getElementById('nav-container');

    searchInput.addEventListener('focusin', () => {
        navContainer.style.transform = `translateY(0)`;
        if (searchInput.value.length > 0)
            setTimeout(_ => { instance.triggerSearch(searchInput.value); }, 400);
    });
    searchInput.addEventListener('beforeinput', () => {
        navContainer.style.transform = `translateY(0)`;
    });
    searchInput.addEventListener('keydown', function (event) {
        if (event.key === "Enter" && !!top_hit) {
            window.location = top_hit;
            top_hit = null;
        }
    });

});
