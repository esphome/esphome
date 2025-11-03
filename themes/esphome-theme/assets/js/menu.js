
const bodystyle = window.getComputedStyle(document.body);
const mobileWidthStop = parseInt(bodystyle.getPropertyValue('--mobile-width-stop'));
function isMobileScreen() {
    return (window.innerWidth <= mobileWidthStop);
}

function closeMenu() {
    const hamburger = document.querySelector('.hamburger-button');
    const navLinks = document.querySelector('.nav-links');
    navLinks.classList.remove('active');
    hamburger.classList.remove('active');
    hamburger.setAttribute('aria-expanded', 'false');
}

function openMenu() {
    const hamburger = document.querySelector('.hamburger-button');
    const navLinks = document.querySelector('.nav-links');
    navLinks.classList.add('active');
    hamburger.classList.add('active');
    hamburger.setAttribute('aria-expanded', 'true');
}
function openTOC() {
    const tocToggle = document.getElementById('toc-toggle');
    if (!isMobileScreen() || !tocToggle) return;
    const tocPanel = document.getElementsByClassName('sidebar-mobile')[0];
    const overlay = document.getElementById('overlay');
    tocToggle.classList.add('open');
    tocPanel.classList.add('open');
    overlay.classList.add('show');
}

function closeTOC() {
    const tocToggle = document.getElementById('toc-toggle');
    if (!isMobileScreen() || !tocToggle) return;
    const tocPanel = document.getElementsByClassName('sidebar-mobile')[0];
    const overlay = document.getElementById('overlay');
    tocToggle.classList.remove('open');
    tocPanel.classList.remove('open');
    overlay.classList.remove('show');
}

document.addEventListener('DOMContentLoaded', function() {
    function setTocSort(sort) {
        document.documentElement.setAttribute('data-toc-sort', sort);
        localStorage.setItem('toc-sort', sort);
        closeMenu();
    }

    const sortToggles = document.getElementsByClassName('toc-sort-button');
    Array.from(sortToggles).forEach(toggle => {
        toggle.addEventListener('click', event => {
            event.stopPropagation();
            const currentSort = document.documentElement.getAttribute('data-toc-sort');
            setTocSort(currentSort === 'alphabetic' ? 'linear' : 'alphabetic');
        });
    });


    function setTheme(theme) {
        document.documentElement.setAttribute('data-theme', theme);
        localStorage.setItem('theme', theme);
        document.querySelector('.theme-toggle').setAttribute('aria-label', `Toggle ${theme === 'dark' ? 'light' : 'dark'} mode`);
        closeMenu();
    }

    // Theme toggle functionality
    const themeToggle = document.querySelector('.theme-toggle');
    themeToggle.addEventListener('click', () => {
        const currentTheme = document.documentElement.getAttribute('data-theme');
        setTheme(currentTheme === 'dark' ? 'light' : 'dark');
    });

    const tocToggle = document.getElementById('toc-toggle');
    const overlay = document.getElementById('overlay');
    if (tocToggle)
        tocToggle.addEventListener('click',_ => {
            if (tocToggle.classList.contains("open"))
                closeTOC();
            else
                openTOC();
        });
    if (overlay)
        overlay.addEventListener('click', closeTOC);
    const sidebarMobile = document.querySelectorAll('.sidebar-mobile');
    sidebarMobile.forEach(sidebar => {
        sidebar.addEventListener("click", closeTOC);
    })

    const dropdownButtons = document.querySelectorAll('.dropdown button');

    dropdownButtons.forEach(button => {
        // Handle Enter and Space key presses
        button.addEventListener('keydown', function(e) {
            if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault();
                toggleDropdown(this);
            }
        });

        // Handle click events
        button.addEventListener('click', function(e) {
            if (!isMobileScreen()) return;
            e.preventDefault();
            // Close others
            dropdownButtons.forEach(function(otherBtn) {
                if (otherBtn !== button) {
                    otherBtn.setAttribute('aria-expanded', 'false');
                    if (otherBtn.nextElementSibling) {
                        otherBtn.nextElementSibling.style.display = 'none';
                    }
                }
            });
            // Toggle this one
            const expanded = button.getAttribute('aria-expanded') === 'true';
            button.setAttribute('aria-expanded', expanded ? "false" : "true");
            if (button.nextElementSibling) {
                button.nextElementSibling.style.display = expanded ? 'none' : 'block';
            }
        });
    });

    // Close dropdowns when Escape key is pressed
    document.addEventListener('keydown', e => {
        if (e.key === 'Escape') {
            closeAllDropdowns();
            closeTOC();
        }
    });

    // Close dropdowns when clicking outside
    document.addEventListener('click', function(e) {
        if (!e.target.matches('.dropbtn')) {
            closeAllDropdowns();
        }
    });

    // Toggle dropdown function
    function toggleDropdown(button) {
        if (window.innerWidth > mobileWidthStop) return;
        const isExpanded = button.getAttribute('aria-expanded') === 'true';
        closeAllDropdowns();

        if (!isExpanded) {
            button.setAttribute('aria-expanded', 'true');
            const dropdownContent = button.nextElementSibling;
            dropdownContent.style.display = 'block';
        }
    }

    // Close all dropdowns
    function closeAllDropdowns() {
        if (window.innerWidth > mobileWidthStop) return;
        dropdownButtons.forEach(btn => {
            btn.setAttribute('aria-expanded', 'false');
            const dropdownContent = btn.nextElementSibling;
            if (dropdownContent)
                dropdownContent.style.display = 'none';
        });
    }


    const hamburger = document.querySelector('.hamburger-button');
    const navLinks = document.querySelector('.nav-links');
    if (!hamburger || !navLinks) return;

    hamburger.addEventListener('click', function(e) {
        e.preventDefault();
        e.stopPropagation();
        hamburger.classList.toggle('active');
        navLinks.classList.toggle('active');
        const expanded = hamburger.getAttribute('aria-expanded') === 'true';
        hamburger.setAttribute('aria-expanded', expanded ? "false" : "true");
        closeTOC();
    });
    // Close menu on outside click (mobile only)
    document.addEventListener('click', function(e) {
        if (window.innerWidth > mobileWidthStop) return;
        if (!e.target.closest('.hamburger-button') && !e.target.closest('.nav-links')) {
            closeMenu();
        }
    });
    // Close menu on resize to desktop
    window.addEventListener('resize', function() {
        if (window.innerWidth > mobileWidthStop) {
            closeMenu();
        }
    });
});
