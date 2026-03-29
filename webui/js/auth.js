// Auth module — login/logout via session cookies

(function() {
    const keyInput = document.getElementById('loginKey');
    const loginBtn = document.getElementById('loginBtn');
    const errorEl = document.getElementById('loginError');

    if (!loginBtn) return; // Not on login page

    async function doLogin() {
        const key = keyInput.value.trim();
        if (!key) { errorEl.textContent = 'Enter API key'; return; }

        errorEl.textContent = '';
        loginBtn.disabled = true;
        loginBtn.textContent = '...';

        try {
            const res = await fetch('/webui/login', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ key: key })
            });

            if (res.ok) {
                window.location.href = '/';
            } else {
                const data = await res.json();
                errorEl.textContent = data.error || 'Login failed';
            }
        } catch (e) {
            errorEl.textContent = 'Connection error';
        } finally {
            loginBtn.disabled = false;
            loginBtn.textContent = 'Login';
        }
    }

    loginBtn.addEventListener('click', doLogin);
    keyInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') doLogin();
    });
})();
