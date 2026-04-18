const API = {
    token: localStorage.getItem('col_token'),

    async request(path, opts = {}) {
        const headers = { 'Content-Type': 'application/json' };
        if (this.token) headers['Authorization'] = 'Bearer ' + this.token;

        const resp = await fetch(path, { ...opts, headers: { ...headers, ...opts.headers } });
        const data = await resp.json();
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
