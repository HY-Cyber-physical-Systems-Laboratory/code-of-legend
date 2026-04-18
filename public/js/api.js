const API = {
    token: localStorage.getItem('col_token'),

    async request(path, opts = {}) {
        const headers = { 'Content-Type': 'application/json' };
        if (this.token) headers['Authorization'] = 'Bearer ' + this.token;

        const ac = new AbortController();
        const timer = setTimeout(function () { ac.abort(); }, 8000);

        try {
            var resp = await fetch(path, { ...opts, signal: ac.signal, headers: { ...headers, ...opts.headers } });
        } catch (e) {
            clearTimeout(timer);
            if (e.name === 'AbortError') throw new Error('Server not responding (DB may be down)');
            throw new Error('Network error');
        }
        clearTimeout(timer);

        var data;
        try { data = await resp.json(); } catch (e) { throw new Error('Invalid server response'); }
        if (!resp.ok) throw new Error(data.error || 'request failed');
        return data;
    },

    async register(username, email, password) {
        const data = await this.request('/api/auth/register', {
            method: 'POST',
            body: JSON.stringify({ username, email, password })
        });
        this.setToken(data.token);
        return data;
    },

    async login(username, password) {
        const data = await this.request('/api/auth/login', {
            method: 'POST',
            body: JSON.stringify({ username, password })
        });
        this.setToken(data.token);
        return data;
    },

    async me() {
        return this.request('/api/auth/me');
    },

    setToken(t) {
        this.token = t;
        localStorage.setItem('col_token', t);
    },

    logout() {
        this.token = null;
        localStorage.removeItem('col_token');
        location.href = '/';
    },

    connectWs() {
        const proto = location.protocol === 'https:' ? 'wss' : 'ws';
        return new WebSocket(proto + '://' + location.host + '/ws/competition?token=' + this.token);
    }
};
