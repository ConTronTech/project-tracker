// Editor module — file viewing, editing, creation

const Editor = {
    pendingFileName: null,

    async openFile(slug, filename) {
        App.currentFile = filename;
        App.setActiveTab(filename);

        const text = await App.apiText(`/projects/${slug}/files/${filename}`);
        if (text === null) return;

        document.getElementById('content').innerHTML = `
            <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:8px">
                <span style="font-size:13px;color:#8a6b78">${App.escapeHtml(filename)}</span>
                <div class="file-actions">
                    <button class="btn-save" data-action="save-file">Save</button>
                    <button class="btn-download" data-action="download-file" data-slug="${slug}" data-file="${App.escapeHtml(filename)}">Download</button>
                    <button class="btn-delete" data-action="delete-file" data-slug="${slug}" data-file="${App.escapeHtml(filename)}">Delete</button>
                </div>
            </div>
            <textarea class="editor" id="editor">${App.escapeHtml(text)}</textarea>
        `;
    },

    async saveFile() {
        if (!App.currentSlug || !App.currentFile) return;
        const file = App.currentFile;
        const text = document.getElementById('editor').value;

        const res = await App.api(`/projects/${App.currentSlug}/files/${file}`, {
            method: 'PUT',
            body: text
        });

        if (res && res.ok) {
            // Flash green border
            const editor = document.getElementById('editor');
            if (editor) {
                editor.style.borderColor = '#ff69b4';
                setTimeout(() => { editor.style.borderColor = '#3d1a2a'; }, 1000);
            }
            // Refresh tabs
            Projects.select(App.currentSlug);
            // Re-open the file after refresh
            setTimeout(() => this.openFile(App.currentSlug, file), 200);
        }
    },

    downloadFile(slug, filename) {
        window.open(`/api/projects/${slug}/files/${filename}/download`, '_blank');
    },

    async deleteFile(slug, filename) {
        if (!confirm(`Delete ${filename}?`)) return;
        await App.api(`/projects/${slug}/files/${filename}`, { method: 'DELETE' });
        Projects.select(slug);
    },

    showNewFile() {
        document.getElementById('nf_name').value = '';
        document.getElementById('nf_conflict').style.display = 'none';
        this.pendingFileName = null;
        App.showModal('newFileModal');
    },

    async createFile() {
        if (!App.currentSlug) return;
        let name = document.getElementById('nf_name').value.trim();
        if (!name) return;
        if (!name.includes('.')) name += '.md';
        this.pendingFileName = name;

        // Check if exists
        const res = await App.api(`/projects/${App.currentSlug}/files/${name}`);
        if (res && res.ok) {
            document.getElementById('nf_conflict_name').textContent = name;
            document.getElementById('nf_conflict').style.display = 'block';
            return;
        }

        await this.doCreateFile(name);
    },

    async resolveConflict(action) {
        let name = this.pendingFileName;
        if (action === 'copy') {
            const ext = name.lastIndexOf('.');
            const base = ext > 0 ? name.substring(0, ext) : name;
            const suffix = ext > 0 ? name.substring(ext) : '.md';
            let num = 2;
            while (num < 100) {
                const tryName = `${base}-${num}${suffix}`;
                const res = await App.api(`/projects/${App.currentSlug}/files/${tryName}`);
                if (!res || !res.ok) { name = tryName; break; }
                num++;
            }
        }
        await this.doCreateFile(name);
    },

    async doCreateFile(name) {
        await App.api(`/projects/${App.currentSlug}/files/${name}`, {
            method: 'PUT',
            body: '# ' + name.replace('.md', '') + '\n'
        });

        this.closeNewFile();
        App.currentFile = name;
        Projects.select(App.currentSlug);
        setTimeout(() => this.openFile(App.currentSlug, name), 200);
    },

    closeNewFile() {
        document.getElementById('nf_name').value = '';
        document.getElementById('nf_conflict').style.display = 'none';
        this.pendingFileName = null;
        App.closeModal('newFileModal');
    },

    importFile() {
        if (!App.currentSlug) return;
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.md,.json,.txt,.yaml,.yml';
        input.onchange = async (e) => {
            const file = e.target.files[0];
            if (!file) return;
            const text = await file.text();
            const res = await App.api(`/projects/${App.currentSlug}/files/${file.name}`, {
                method: 'PUT',
                body: text
            });
            if (res && res.ok) Projects.select(App.currentSlug);
        };
        input.click();
    }
};

// Event bindings
document.getElementById('newFileBtn')?.addEventListener('click', () => Editor.showNewFile());
document.getElementById('createFileBtn')?.addEventListener('click', () => Editor.createFile());
document.getElementById('closeNewFileBtn')?.addEventListener('click', () => Editor.closeNewFile());
document.getElementById('importFileBtn')?.addEventListener('click', () => Editor.importFile());
document.getElementById('conflictOverwrite')?.addEventListener('click', () => Editor.resolveConflict('overwrite'));
document.getElementById('conflictCopy')?.addEventListener('click', () => Editor.resolveConflict('copy'));
