(function () {
    if (!API.token) { location.href = '/'; return; }

    var match = JSON.parse(sessionStorage.getItem('col_match'));
    if (!match) { location.href = '/main.html'; return; }

    // ── Problem definitions ──
    var PROBLEMS = [
        {
            id: 0, title: 'A + B', difficulty: 'easy', damage: 15, stars: '\u2605',
            desc: '<p>두 정수 A와 B를 읽고, A + B를 출력하세요.</p>',
            input: '<p>두 정수 A, B가 공백으로 구분되어 주어진다. (-10<sup>9</sup> \u2264 A, B \u2264 10<sup>9</sup>)</p>',
            output: '<p>A + B를 출력한다.</p>',
            examples: [{ in: '1 2', out: '3' }, { in: '-5 3', out: '-2' }]
        },
        {
            id: 1, title: 'Reverse String', difficulty: 'easy', damage: 15, stars: '\u2605',
            desc: '<p>문자열 S를 읽고, 뒤집어서 출력하세요.</p>',
            input: '<p>문자열 S (1 \u2264 |S| \u2264 1000, 공백 없음)</p>',
            output: '<p>S를 뒤집은 문자열을 출력한다.</p>',
            examples: [{ in: 'hello', out: 'olleh' }, { in: 'racecar', out: 'racecar' }]
        },
        {
            id: 2, title: 'Max Value', difficulty: 'medium', damage: 30, stars: '\u2605\u2605',
            desc: '<p>N개의 정수가 주어질 때, 최댓값을 출력하세요.</p>',
            input: '<p>첫 줄에 N (1 \u2264 N \u2264 10<sup>5</sup>)<br>둘째 줄에 N개의 정수 (-10<sup>9</sup> \u2264 a<sub>i</sub> \u2264 10<sup>9</sup>)</p>',
            output: '<p>최댓값을 출력한다.</p>',
            examples: [{ in: '5\n3 1 4 1 5', out: '5' }, { in: '3\n-1 -5 -3', out: '-1' }]
        },
        {
            id: 3, title: 'Sort', difficulty: 'medium', damage: 30, stars: '\u2605\u2605',
            desc: '<p>N개의 정수를 오름차순으로 정렬하여 출력하세요.</p>',
            input: '<p>첫 줄에 N (1 \u2264 N \u2264 10<sup>5</sup>)<br>둘째 줄에 N개의 정수</p>',
            output: '<p>정렬된 수를 공백으로 구분하여 출력한다.</p>',
            examples: [{ in: '5\n5 3 1 4 2', out: '1 2 3 4 5' }]
        },
        {
            id: 4, title: 'Max Subarray', difficulty: 'hard', damage: 50, stars: '\u2605\u2605\u2605',
            desc: '<p>N개의 정수로 이루어진 수열에서 연속된 부분 수열의 합 중 최댓값을 구하세요.</p>',
            input: '<p>첫 줄에 N (1 \u2264 N \u2264 10<sup>5</sup>)<br>둘째 줄에 N개의 정수 (-10<sup>9</sup> \u2264 a<sub>i</sub> \u2264 10<sup>9</sup>)</p>',
            output: '<p>최대 부분합을 출력한다.</p>',
            examples: [{ in: '8\n-2 1 -3 4 -1 2 1 -5', out: '6' }, { in: '5\n-1 -2 -3 -4 -5', out: '-1' }]
        },
        {
            id: 5, title: 'Count Primes', difficulty: 'hard', damage: 50, stars: '\u2605\u2605\u2605',
            desc: '<p>N 이하의 소수의 개수를 구하세요.</p>',
            input: '<p>정수 N (1 \u2264 N \u2264 10<sup>6</sup>)</p>',
            output: '<p>N 이하의 소수의 개수를 출력한다.</p>',
            examples: [{ in: '10', out: '4' }, { in: '30', out: '10' }]
        }
    ];

    var TEMPLATES = {
        python: 'import sys\ninput = sys.stdin.readline\n\n'
    };

    // ── DOM refs ──
    var playerCards = document.getElementById('player-cards');
    var problemTabs = document.getElementById('problem-tabs');
    var problemBody = document.getElementById('problem-body');
    var timerEl = document.getElementById('timer');
    var submitBtn = document.getElementById('submit-btn');
    var langSelect = document.getElementById('lang-select');
    var leaveBtn = document.getElementById('leave-btn');
    var resetBtn = document.getElementById('reset-btn');
    var statusEl = document.getElementById('editor-status');
    var judgeMsg = document.getElementById('judge-msg');

    var editor = null;
    var currentProblem = 0;
    var myUsername = '';
    var hpState = {};
    var solvedSet = {};

    // ── Get my username ──
    API.me().then(function (u) { myUsername = u.username; renderPlayers(); });

    // ── Build problem tabs ──
    PROBLEMS.forEach(function (p, i) {
        var btn = document.createElement('button');
        btn.className = 'prob-tab diff-' + p.difficulty;
        btn.dataset.id = i;
        btn.innerHTML = '<span class="prob-check" id="check-' + i + '"></span>' +
            p.title + ' <span class="prob-dmg">' + p.damage + 'dmg</span>';
        btn.onclick = function () { selectProblem(i); };
        problemTabs.appendChild(btn);
    });

    function selectProblem(idx) {
        currentProblem = idx;
        var p = PROBLEMS[idx];
        document.querySelectorAll('.prob-tab').forEach(function (t) { t.classList.remove('active'); });
        document.querySelector('.prob-tab[data-id="' + idx + '"]').classList.add('active');

        var html = '<h2>' + p.title + ' <span class="diff-badge diff-' + p.difficulty + '">' +
            p.stars + ' ' + p.difficulty.toUpperCase() + ' (' + p.damage + ' dmg)</span></h2>';
        html += '<h4>Description</h4>' + p.desc;
        html += '<h4>Input</h4>' + p.input;
        html += '<h4>Output</h4>' + p.output;
        html += '<h4>Examples</h4>';
        p.examples.forEach(function (ex) {
            html += '<div class="example-box"><div class="ex-col"><b>Input</b><pre>' + ex.in +
                '</pre></div><div class="ex-col"><b>Output</b><pre>' + ex.out + '</pre></div></div>';
        });
        problemBody.innerHTML = html;
    }
    selectProblem(0);

    // ── Build player cards with HP bars ──
    function renderPlayers() {
        playerCards.innerHTML = '';
        match.players.forEach(function (p) {
            var isMe = p.username === myUsername;
            var hp = hpState[p.username] || { hp: 100, score: 0, alive: true };
            var pct = Math.max(0, hp.hp);
            var barColor = pct > 60 ? '#10b981' : pct > 30 ? '#f59e0b' : '#ef4444';

            var div = document.createElement('div');
            div.className = 'player-card' + (isMe ? ' is-me' : '') + (!hp.alive ? ' is-dead' : '');
            div.id = 'pcard-' + p.username;
            div.innerHTML =
                '<div class="pc-header">' +
                    '<span class="pc-name">' + (isMe ? '\u25C6 ' : '') + p.username + '</span>' +
                    '<span class="pc-rating">' + p.rating + '</span>' +
                '</div>' +
                '<div class="hp-bar-bg"><div class="hp-bar-fill" id="hpbar-' + p.username +
                    '" style="width:' + pct + '%;background:' + barColor + '"></div></div>' +
                '<div class="pc-stats">' +
                    '<span class="pc-hp" id="hpval-' + p.username + '">HP ' + hp.hp + '/100</span>' +
                    '<span class="pc-score" id="scoreval-' + p.username + '">Score ' + hp.score + '</span>' +
                '</div>';
            playerCards.appendChild(div);
        });
    }
    renderPlayers();

    function updateHp(players) {
        hpState = players;
        Object.keys(players).forEach(function (u) {
            var st = players[u];
            var pct = Math.max(0, st.hp);
            var barColor = pct > 60 ? '#10b981' : pct > 30 ? '#f59e0b' : '#ef4444';

            var bar = document.getElementById('hpbar-' + u);
            var hpVal = document.getElementById('hpval-' + u);
            var scoreVal = document.getElementById('scoreval-' + u);
            var card = document.getElementById('pcard-' + u);

            if (bar) { bar.style.width = pct + '%'; bar.style.background = barColor; }
            if (hpVal) hpVal.textContent = 'HP ' + st.hp + '/100';
            if (scoreVal) scoreVal.textContent = 'Score ' + st.score;
            if (card && !st.alive) card.classList.add('is-dead');
        });
    }

    function showDamage(username, dmg) {
        var card = document.getElementById('pcard-' + username);
        if (!card) return;
        card.classList.add('hit-shake');
        setTimeout(function () { card.classList.remove('hit-shake'); }, 400);

        var popup = document.createElement('div');
        popup.className = 'damage-popup';
        popup.textContent = '-' + dmg;
        card.appendChild(popup);
        setTimeout(function () { popup.remove(); }, 1000);
    }

    function markSolved(problemId) {
        solvedSet[problemId] = true;
        var chk = document.getElementById('check-' + problemId);
        if (chk) chk.textContent = '\u2713 ';
        var tab = document.querySelector('.prob-tab[data-id="' + problemId + '"]');
        if (tab) tab.classList.add('solved');
    }

    // ── Monaco Editor ──
    require.config({
        paths: { vs: 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.44.0/min/vs' }
    });

    require(['vs/editor/editor.main'], function () {
        monaco.editor.defineTheme('col-dark', {
            base: 'vs-dark', inherit: true, rules: [],
            colors: {
                'editor.background': '#0d0d14',
                'editor.lineHighlightBackground': '#1a1a2e',
                'editorLineNumber.foreground': '#4a4a6a',
                'editorLineNumber.activeForeground': '#818cf8',
                'editor.selectionBackground': '#6366f140',
                'editorCursor.foreground': '#818cf8'
            }
        });

        editor = monaco.editor.create(document.getElementById('monaco-container'), {
            value: TEMPLATES.python,
            language: 'python',
            theme: 'col-dark',
            fontSize: 14,
            fontFamily: "'Fira Code','Consolas','Courier New',monospace",
            minimap: { enabled: false },
            scrollBeyondLastLine: false,
            automaticLayout: true,
            tabSize: 4,
            insertSpaces: true,
            renderLineHighlight: 'line',
            cursorBlinking: 'smooth',
            cursorSmoothCaretAnimation: 'on',
            smoothScrolling: true,
            padding: { top: 12, bottom: 12 },
            bracketPairColorization: { enabled: true }
        });

        editor.addAction({
            id: 'submit-code', label: 'Submit',
            keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter],
            run: function () { submitBtn.click(); }
        });

        editor.onDidChangeCursorPosition(function (e) {
            statusEl.textContent = 'Ln ' + e.position.lineNumber + ', Col ' + e.position.column;
        });
        statusEl.textContent = 'Ln 1, Col 1';
    });

    resetBtn.addEventListener('click', function () {
        if (editor) { editor.setValue(TEMPLATES.python); editor.focus(); }
    });

    // ── WebSocket ──
    var ws = API.connectWs();
    var gameEnded = false;

    ws.onopen = function () {
        ws.send(JSON.stringify({ action: 'join_room', room_id: match.room_id }));
    };

    ws.onmessage = function (e) {
        var msg = JSON.parse(e.data);

        if (msg.type === 'hp_update') {
            updateHp(msg.players);
        }
        else if (msg.type === 'judge_result') {
            var isMe = msg.username === myUsername;

            if (msg.verdict === 'judging') {
                if (isMe) {
                    judgeMsg.textContent = 'Judging...';
                    judgeMsg.className = 'judge-msg judging';
                }
            }
            else if (msg.verdict === 'accepted') {
                if (isMe) {
                    judgeMsg.textContent = 'Accepted! (-' + msg.damage + ' dmg)';
                    judgeMsg.className = 'judge-msg accepted';
                    markSolved(msg.problem_id);
                    submitBtn.disabled = false;
                    submitBtn.textContent = 'Submit';
                }
                // Show damage on opponents
                match.players.forEach(function (p) {
                    if (p.username !== msg.username) {
                        showDamage(p.username, msg.damage);
                    }
                });
            }
            else {
                if (isMe) {
                    var labels = {
                        wrong_answer: 'Wrong Answer',
                        tle: 'Time Limit Exceeded',
                        runtime_error: 'Runtime Error'
                    };
                    judgeMsg.textContent = labels[msg.verdict] || msg.verdict;
                    judgeMsg.className = 'judge-msg failed';
                    submitBtn.disabled = false;
                    submitBtn.textContent = 'Submit';
                }
            }
        }
        else if (msg.type === 'already_solved') {
            judgeMsg.textContent = 'Already solved!';
            judgeMsg.className = 'judge-msg failed';
            submitBtn.disabled = false;
            submitBtn.textContent = 'Submit';
        }
        else if (msg.type === 'game_over') {
            gameEnded = true;
            var isWinner = msg.winner === myUsername;
            var myScore = hpState[myUsername] ? hpState[myUsername].score : 0;

            if (msg.reason === 'kill') {
                if (isWinner) {
                    document.getElementById('victory-score').textContent = myScore;
                    document.getElementById('victory-overlay').classList.remove('hidden');
                } else {
                    document.getElementById('death-score').textContent = myScore;
                    document.getElementById('death-overlay').classList.remove('hidden');
                }
            } else {
                var title = document.getElementById('timeover-title');
                var tmsg = document.getElementById('timeover-msg');
                document.getElementById('timeover-score').textContent = myScore;

                if (isWinner) {
                    title.textContent = 'Victory!';
                    title.className = 'victory-title';
                    tmsg.textContent = '시간 종료 - 점수로 승리!';
                } else if (msg.winner === '') {
                    title.textContent = 'Draw';
                    title.className = 'draw-title';
                    tmsg.textContent = '시간 종료 - 무승부';
                } else {
                    title.textContent = '패배';
                    title.className = 'death-title';
                    tmsg.textContent = '시간 종료 - 점수 부족';
                }
                document.getElementById('timeover-overlay').classList.remove('hidden');
            }
        }
        else if (msg.type === 'player_left') {
            var card = document.getElementById('pcard-' + msg.username);
            if (card) card.style.opacity = '0.3';
        }
    };

    // ── Submit ──
    submitBtn.addEventListener('click', function () {
        if (!editor || gameEnded) return;
        var code = editor.getValue().trim();
        if (!code) return;
        if (solvedSet[currentProblem]) {
            judgeMsg.textContent = 'Already solved!';
            judgeMsg.className = 'judge-msg failed';
            return;
        }

        ws.send(JSON.stringify({
            action: 'submit',
            room_id: match.room_id,
            problem_id: currentProblem,
            language: langSelect.value,
            code: code
        }));

        submitBtn.disabled = true;
        submitBtn.textContent = 'Judging...';
        judgeMsg.textContent = '';
    });

    // ── 10 min timer ──
    var remaining = 600;
    var tick = setInterval(function () {
        remaining--;
        var m = Math.floor(remaining / 60);
        var s = remaining % 60;
        timerEl.textContent = String(m).padStart(2, '0') + ':' + String(s).padStart(2, '0');

        if (remaining <= 60) timerEl.classList.add('danger');
        if (remaining <= 0) {
            clearInterval(tick);
            timerEl.textContent = "TIME'S UP";
            if (!gameEnded) {
                ws.send(JSON.stringify({ action: 'time_up', room_id: match.room_id }));
            }
        }
    }, 1000);

    // ── Leave ──
    leaveBtn.addEventListener('click', function () {
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ action: 'leave_room', room_id: match.room_id }));
            ws.close();
        }
        sessionStorage.removeItem('col_match');
        location.href = '/main.html';
    });

    window.addEventListener('beforeunload', function () {
        if (ws.readyState === WebSocket.OPEN) ws.close();
    });
})();
