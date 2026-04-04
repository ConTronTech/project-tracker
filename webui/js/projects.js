// Projects module — list, search, CRUD

const Projects = {
    async loadList() {
        const q = document.getElementById('search').value;
        const showArchived = document.getElementById('showArchived')?.checked;
        
        // Single request — use status=all when showing archived
        let url = '/projects';
        const params = [];
        if (q) params.push(`search=${encodeURIComponent(q)}`);
        if (showArchived) params.push('status=all');
        if (params.length) url += '?' + params.join('&');
        
        let projects = await App.apiJson(url) || [];

        const statusOrder = { active: 0, paused: 1, completed: 2, archived: 3, abandoned: 4 };
        const prioOrder = { high: 0, medium: 1, low: 2 };
        projects.sort((a, b) => {
            const s = (statusOrder[a.status] ?? 9) - (statusOrder[b.status] ?? 9);
            if (s !== 0) return s;
            return (prioOrder[a.priority] ?? 9) - (prioOrder[b.priority] ?? 9);
        });

        const list = document.getElementById('projectList');
        list.innerHTML = projects.map(p => {
            const tags = p.tags ? p.tags.split(',').filter(t => t.trim()).map(t =>
                `<span class="tag-pill">${App.escapeHtml(t.trim())}</span>`
            ).join('') : '';
            return `
            <div class="project-item ${p.slug === App.currentSlug ? 'active' : ''}"
                 data-action="select-project" data-slug="${App.escapeHtml(p.slug)}">
                <div class="name">${App.escapeHtml(p.name)} <span class="badge ${p.status}">${p.status}</span></div>
                <div class="meta"><div class="tag-row">${tags}</div><span class="file-count">${p.file_count} files</span></div>
            </div>`;
        }).join('');
    },

    async select(slug) {
        App.currentSlug = slug;
        App.currentFile = null;
        this.loadList();

        const p = await App.apiJson(`/projects/${slug}`);
        if (!p) return;

        document.getElementById('toolbar').style.display = 'flex';
        document.getElementById('toolbarTitle').textContent = p.name;
        this.updateArchiveButton(p.status);

        const tabs = document.getElementById('tabs');
        tabs.style.display = 'flex';
        tabs.innerHTML = `<div class="tab active" data-action="show-info" data-slug="${slug}">Info</div>` +
            p.files.map(f => `<div class="tab" data-action="open-file" data-slug="${slug}" data-file="${App.escapeHtml(f.filename)}">${App.escapeHtml(f.filename)}</div>`).join('');

        this.showInfo(slug);
    },

    async showInfo(slug) {
        const p = await App.apiJson(`/projects/${slug}`);
        if (!p) return;
        App.setActiveTab('Info');

        document.getElementById('content').innerHTML = `
            <div class="detail-grid">
                <div class="detail-item"><label>Slug</label><div class="value">${App.escapeHtml(p.slug)}</div></div>
                <div class="detail-item"><label>Status</label><div class="value"><span class="badge ${p.status}">${p.status}</span></div></div>
                <div class="detail-item"><label>Priority</label><div class="value">${App.escapeHtml(p.priority)}</div></div>
                <div class="detail-item"><label>Tags</label><div class="value">${App.escapeHtml(p.tags)}</div></div>
                <div class="detail-item"><label>Description</label><div class="value">${App.escapeHtml(p.description)}</div></div>
                <div class="detail-item"><label>Repo</label><div class="value">${App.escapeHtml(p.repo_path || 'N/A')}</div></div>
                <div class="detail-item"><label>Created</label><div class="value">${App.escapeHtml(p.created_at)}</div></div>
                <div class="detail-item"><label>Updated</label><div class="value">${App.escapeHtml(p.updated_at)}</div></div>
            </div>
            <h3 class="files-heading">Files (${p.files.length})</h3>
            <div class="file-list">
            ${p.files.map(f => `
                <div class="file-item" data-action="open-file" data-slug="${slug}" data-file="${App.escapeHtml(f.filename)}">
                    <span class="fname">${App.escapeHtml(f.filename)}</span>
                    <span class="fsize">${f.size} bytes · ${App.escapeHtml(f.updated_at)}</span>
                </div>
            `).join('') || '<div style="color:#8a6b78">No files yet</div>'}
            </div>
        `;
    },

    async create() {
        const body = {
            slug: document.getElementById('np_slug').value.trim(),
            name: document.getElementById('np_name').value.trim(),
            tags: document.getElementById('np_tags').value.trim(),
            description: document.getElementById('np_desc').value.trim(),
            repo_path: document.getElementById('np_repo').value.trim(),
            priority: document.getElementById('np_priority').value
        };
        if (!body.slug) return;

        await App.api('/projects', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });

        App.closeModal('newProjectModal');
        // Clear inputs
        ['np_slug', 'np_name', 'np_tags', 'np_desc', 'np_repo'].forEach(id =>
            document.getElementById(id).value = ''
        );
        this.loadList();
    },

    async edit() {
        const p = await App.apiJson(`/projects/${App.currentSlug}`);
        if (!p) return;
        document.getElementById('ep_name').value = p.name || '';
        document.getElementById('ep_status').value = p.status || 'active';
        document.getElementById('ep_priority').value = p.priority || 'medium';
        document.getElementById('ep_tags').value = p.tags || '';
        document.getElementById('ep_desc').value = p.description || '';
        document.getElementById('ep_repo').value = p.repo_path || '';
        App.showModal('editProjectModal');
    },

    async save() {
        const body = {};
        const fields = { name: 'ep_name', status: 'ep_status', priority: 'ep_priority',
                         tags: 'ep_tags', description: 'ep_desc', repo_path: 'ep_repo' };
        for (const [key, id] of Object.entries(fields)) {
            const val = document.getElementById(id).value;
            if (val) body[key] = val;
        }

        await App.api(`/projects/${App.currentSlug}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });

        App.closeModal('editProjectModal');
        this.loadList();
        this.select(App.currentSlug);
    },

    async archive() {
        if (!App.currentSlug) return;
        const p = await App.apiJson(`/projects/${App.currentSlug}`);
        if (!p) return;

        const isArchiving = p.status !== 'archived';
        const newStatus = isArchiving ? 'archived' : 'active';

        // Update modal content
        const icon = document.getElementById('archiveModalIcon');
        const title = document.getElementById('archiveModalTitle');
        const name = document.getElementById('archiveModalName');
        const hint = document.getElementById('archiveModalHint');
        const btn = document.getElementById('archiveConfirmBtn');

        if (isArchiving) {
            icon.textContent = '📦';
            title.textContent = 'Archive Project';
            hint.textContent = 'Archived projects are hidden from the main list but can be restored anytime.';
            btn.textContent = 'Archive';
            btn.className = 'ok archive-confirm-btn archiving';
        } else {
            icon.textContent = '📂';
            title.textContent = 'Restore Project';
            hint.textContent = 'This will move the project back to the active list.';
            btn.textContent = 'Restore';
            btn.className = 'ok archive-confirm-btn unarchiving';
        }
        name.textContent = p.name;

        // Show modal and wait for confirm
        App.showModal('archiveModal');

        // Cleanup function to remove all handlers
        const cleanup = () => {
            btn.removeEventListener('click', confirmHandler);
            cancelBtn.removeEventListener('click', cancelHandler);
        };

        const cancelBtn = document.querySelector('[data-close="archiveModal"]');

        const confirmHandler = async () => {
            cleanup();
            App.closeModal('archiveModal');

            await App.api(`/projects/${App.currentSlug}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ status: newStatus })
            });

            if (newStatus === 'archived') {
                const toggle = document.getElementById('showArchived');
                if (toggle && !toggle.checked) toggle.checked = true;
            }

            this.loadList();
            this.select(App.currentSlug);
            this.updateArchiveButton(newStatus);
        };

        const cancelHandler = () => cleanup();

        btn.addEventListener('click', confirmHandler);
        cancelBtn.addEventListener('click', cancelHandler);
    },

    updateArchiveButton(status) {
        const btn = document.getElementById('archiveProjectBtn');
        if (!btn) return;
        if (status === 'archived') {
            btn.textContent = 'Unarchive';
            btn.classList.add('unarchive');
            btn.classList.remove('archive');
        } else {
            btn.textContent = 'Archive';
            btn.classList.add('archive');
            btn.classList.remove('unarchive');
        }
    },

    async remove() {
        if (!confirm(`Delete ${App.currentSlug} and ALL its files?`)) return;

        await App.api(`/projects/${App.currentSlug}`, { method: 'DELETE' });
        App.currentSlug = null;
        document.getElementById('toolbar').style.display = 'none';
        document.getElementById('tabs').style.display = 'none';
        document.getElementById('content').innerHTML = '<div class="empty">Select a project</div>';
        this.loadList();
    }
};

// Event bindings
document.getElementById('search')?.addEventListener('input', () => Projects.loadList());
document.getElementById('newProjectBtn')?.addEventListener('click', () => App.showModal('newProjectModal'));
document.getElementById('createProjectBtn')?.addEventListener('click', () => Projects.create());
document.getElementById('editProjectBtn')?.addEventListener('click', () => Projects.edit());
document.getElementById('archiveProjectBtn')?.addEventListener('click', () => Projects.archive());
document.getElementById('saveProjectBtn')?.addEventListener('click', () => Projects.save());
document.getElementById('deleteProjectBtn')?.addEventListener('click', () => Projects.remove());
document.getElementById('showArchived')?.addEventListener('change', () => Projects.loadList());

// Initial load
Projects.loadList();
