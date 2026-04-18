(function () {
    if (!API.token) { location.href = '/'; return; }

    const usernameEl = document.getElementById('username');
    const ratingEl = document.getElementById('rating');
    const matchBtn = document.getElementById('match-btn');
    const queueBox = document.getElementById('queue-box');
    const cancelBtn = document.getElementById('cancel-btn');

    let ws = null;

    API.me().then(user => {
        usernameEl.textContent = user.username;
        ratingEl.textContent = 'Rating ' + user.rating;
    }).catch(() => API.logout());

    document.getElementById('logout-btn').addEventListener('click', () => API.logout());

    matchBtn.addEventListener('click', () => {
        matchBtn.classList.add('hidden');
        queueBox.classList.remove('hidden');

        ws = API.connectWs();

        ws.onopen = () => {
            ws.send(JSON.stringify({ action: 'queue' }));
        };

        ws.onmessage = e => {
            const msg = JSON.parse(e.data);

            if (msg.type === 'matched') {
                sessionStorage.setItem('col_match', JSON.stringify(msg));
                showMatchFound();
            }
        };

        ws.onclose = () => {
            matchBtn.classList.remove('hidden');
            queueBox.classList.add('hidden');
        };
    });

    cancelBtn.addEventListener('click', () => {
        if (ws) {
            ws.send(JSON.stringify({ action: 'dequeue' }));
            ws.close();
            ws = null;
        }
        matchBtn.classList.remove('hidden');
        queueBox.classList.add('hidden');
    });

    function showMatchFound() {
        const overlay = document.createElement('div');
        overlay.className = 'match-found';
        overlay.innerHTML = '<h1>Match Found!</h1><p>Entering arena...</p>';
        document.body.appendChild(overlay);

        setTimeout(() => {
            if (ws) ws.close();
            location.href = '/arena.html';
        }, 1500);
    }
})();
