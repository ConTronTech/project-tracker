// App core — API wrapper, state, utilities

const App = {
    currentSlug: null,
    currentFile: null,

    // Fetch wrapper — calls API directly, session cookie sent automatically
    async api(path, opts = {}) {
        const url = '/api' + path;
        opts.credentials = 'same-origin'; // Send session cookie
        try {
            const res = await fetch(url, opts);
            if (res.status === 401) {
                window.location.href = '/login';
                return null;
            }
            return res;
        } catch (e) {
            console.error('API error:', e);
            return null;
        }
    },

    async apiJson(path, opts = {}) {
        const res = await this.api(path, opts);
        if (!res) return null;
        try { return await res.json(); } catch { return null; }
    },

    async apiText(path, opts = {}) {
        const res = await this.api(path, opts);
        if (!res) return null;
        try { return await res.text(); } catch { return null; }
    },

    // Modal helpers
    showModal(id) {
        document.getElementById(id).classList.add('show');
    },
    closeModal(id) {
        document.getElementById(id).classList.remove('show');
    },

    // Escape HTML
    escapeHtml(s) {
        const d = document.createElement('div');
        d.textContent = s;
        return d.innerHTML;
    },

    // Set active tab
    setActiveTab(name) {
        document.querySelectorAll('.tab').forEach(t =>
            t.classList.toggle('active', t.textContent === name)
        );
    }
};

// Global event delegation for data-action clicks (CSP-safe, no inline handlers)
document.addEventListener('click', (e) => {
    const el = e.target.closest('[data-action]');
    if (el) {
        const action = el.dataset.action;
        const slug = el.dataset.slug;
        const file = el.dataset.file;
        switch (action) {
            case 'select-project': Projects.select(slug); break;
            case 'show-info': Projects.showInfo(slug); break;
            case 'open-file': Editor.openFile(slug, file); break;
            case 'save-file': Editor.saveFile(); break;
            case 'download-file': Editor.downloadFile(slug, file); break;
            case 'delete-file': Editor.deleteFile(slug, file); break;
        }
    }

    // Close modals via cancel buttons
    if (e.target.dataset.close) {
        App.closeModal(e.target.dataset.close);
    }
    // Close on background click
    if (e.target.classList.contains('modal-bg')) {
        e.target.classList.remove('show');
    }
});

// Escape key closes modals
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        document.querySelectorAll('.modal-bg.show').forEach(m => m.classList.remove('show'));
    }
});

// Logout
document.getElementById('logoutBtn')?.addEventListener('click', async () => {
    await fetch('/webui/logout', { method: 'POST', credentials: 'same-origin' });
    window.location.href = '/login';
});
