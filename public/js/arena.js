(function () {
    if (!API.token) { location.href = '/'; return; }

    const match = JSON.parse(sessionStorage.getItem('col_match'));
    if (!match) { location.href = '/main.html'; return; }

    const playerList = document.getElementById('player-list');
    const timerEl = document.getElementById('timer');
    const codeEditor = document.getElementById('code-editor');
    const submitBtn = document.getElementById('submit-btn');
    const langSelect = document.getElementById('lang-select');
    const leaveBtn = document.getElementById('leave-btn');

    const playerStatus = {};

    match.players.forEach(p => {
        const li = document.createElement('li');
        li.id = 'player-' + p.username;
        li.innerHTML =
            '<span>' + p.username + ' <span class="rating">' + p.rating + '</span></span>' +
            '<span class="status"></span>';
        playerList.appendChild(li);
        playerStatus[p.username] = li.querySelector('.status');
    });

    const ws = API.connectWs();

    ws.onopen = () => {
        ws.send(JSON.stringify({ action: 'join_room', room_id: match.room_id }));
    };

    ws.onmessage = e => {
        const msg = JSON.parse(e.data);

        if (msg.type === 'submission') {
            const el = playerStatus[msg.username];
            if (el) {
                el.textContent = msg.status;
                el.className = 'status status-' + msg.status;
            }
        }
        else if (msg.type === 'player_left') {
            const li = document.getElementById('player-' + msg.username);
            if (li) li.style.opacity = '0.4';
        }
    };

    // 5 min timer
    let remaining = 300;
    const tick = setInterval(() => {
        remaining--;
        const m = Math.floor(remaining / 60);
        const s = remaining % 60;
        timerEl.textContent = String(m).padStart(2, '0') + ':' + String(s).padStart(2, '0');

        if (remaining <= 60) timerEl.classList.add('danger');
        if (remaining <= 0) {
            clearInterval(tick);
            timerEl.textContent = "TIME'S UP";
        }
    }, 1000);

    submitBtn.addEventListener('click', () => {
        const code = codeEditor.value.trim();
        if (!code) return;

        ws.send(JSON.stringify({
            action: 'submit',
            room_id: match.room_id,
            problem_id: 1,
            language: langSelect.value,
            code: code
        }));

        submitBtn.disabled = true;
        submitBtn.textContent = 'Judging...';
        setTimeout(() => {
            submitBtn.disabled = false;
            submitBtn.textContent = 'Submit';
        }, 3000);
    });

    // Tab key in editor
    codeEditor.addEventListener('keydown', e => {
        if (e.key === 'Tab') {
            e.preventDefault();
            const s = codeEditor.selectionStart;
            codeEditor.value = codeEditor.value.substring(0, s) + '    ' + codeEditor.value.substring(codeEditor.selectionEnd);
            codeEditor.selectionStart = codeEditor.selectionEnd = s + 4;
        }
    });

    leaveBtn.addEventListener('click', () => {
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ action: 'leave_room', room_id: match.room_id }));
            ws.close();
        }
        sessionStorage.removeItem('col_match');
        location.href = '/main.html';
    });

    window.addEventListener('beforeunload', () => {
        if (ws.readyState === WebSocket.OPEN) ws.close();
    });
})();
