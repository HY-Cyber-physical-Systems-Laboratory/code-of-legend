(function () {
    if (API.token) { location.href = '/main.html'; return; }

    const tabs = document.querySelectorAll('.tab');
    const loginForm = document.getElementById('login-form');
    const registerForm = document.getElementById('register-form');
    const errorEl = document.getElementById('error-msg');

    tabs.forEach(tab => tab.addEventListener('click', () => {
        tabs.forEach(t => t.classList.remove('active'));
        tab.classList.add('active');
        loginForm.classList.toggle('hidden', tab.dataset.tab !== 'login');
        registerForm.classList.toggle('hidden', tab.dataset.tab !== 'register');
        errorEl.classList.add('hidden');
    }));

    function showError(msg) {
        errorEl.textContent = msg;
        errorEl.classList.remove('hidden');
    }

    loginForm.addEventListener('submit', async e => {
        e.preventDefault();
        errorEl.classList.add('hidden');
        const fd = new FormData(loginForm);
        try {
            await API.login(fd.get('username'), fd.get('password'));
            location.href = '/main.html';
        } catch (err) { showError(err.message); }
    });

    registerForm.addEventListener('submit', async e => {
        e.preventDefault();
        errorEl.classList.add('hidden');
        const fd = new FormData(registerForm);
        try {
            await API.register(fd.get('username'), fd.get('email'), fd.get('password'));
            location.href = '/main.html';
        } catch (err) { showError(err.message); }
    });
})();
