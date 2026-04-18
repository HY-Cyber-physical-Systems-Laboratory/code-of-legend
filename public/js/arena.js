(function () {
    if (!API.token) { location.href = '/'; return; }

    var match = JSON.parse(sessionStorage.getItem('col_match'));
    if (!match) { location.href = '/main.html'; return; }

    var playerList = document.getElementById('player-list');
    var timerEl = document.getElementById('timer');
    var submitBtn = document.getElementById('submit-btn');
    var langSelect = document.getElementById('lang-select');
    var leaveBtn = document.getElementById('leave-btn');
    var resetBtn = document.getElementById('reset-btn');
    var statusEl = document.getElementById('editor-status');

    var editor = null;
    var playerStatus = {};

    var TEMPLATES = {
        cpp: [
            '#include <bits/stdc++.h>',
            'using namespace std;',
            '',
            'int main() {',
            '    ios::sync_with_stdio(false);',
            '    cin.tie(nullptr);',
            '    ',
            '    return 0;',
            '}'
        ].join('\n'),
        python: [
            'import sys',
            'input = sys.stdin.readline',
            '',
            'def solve():',
            '    pass',
            '',
            'solve()'
        ].join('\n'),
        java: [
            'import java.util.*;',
            'import java.io.*;',
            '',
            'public class Main {',
            '    public static void main(String[] args) throws Exception {',
            '        BufferedReader br = new BufferedReader(new InputStreamReader(System.in));',
            '        ',
            '    }',
            '}'
        ].join('\n')
    };

    var LANG_MAP = {
        cpp: 'cpp',
        python: 'python',
        java: 'java'
    };

    // Players
    match.players.forEach(function (p) {
        var li = document.createElement('li');
        li.id = 'player-' + p.username;
        li.innerHTML =
            '<span>' + p.username + ' <span class="rating">' + p.rating + '</span></span>' +
            '<span class="status"></span>';
        playerList.appendChild(li);
        playerStatus[p.username] = li.querySelector('.status');
    });

    // Monaco Editor
    require.config({
        paths: { vs: 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.44.0/min/vs' }
    });

    require(['vs/editor/editor.main'], function () {
        monaco.editor.defineTheme('col-dark', {
            base: 'vs-dark',
            inherit: true,
            rules: [],
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
            value: TEMPLATES.cpp,
            language: 'cpp',
            theme: 'col-dark',
            fontSize: 14,
            fontFamily: "'Fira Code', 'Consolas', 'Courier New', monospace",
            fontLigatures: true,
            minimap: { enabled: false },
            scrollBeyondLastLine: false,
            automaticLayout: true,
            tabSize: 4,
            insertSpaces: true,
            wordWrap: 'off',
            lineNumbers: 'on',
            renderLineHighlight: 'line',
            cursorBlinking: 'smooth',
            cursorSmoothCaretAnimation: 'on',
            smoothScrolling: true,
            padding: { top: 12, bottom: 12 },
            suggest: {
                showKeywords: true,
                showSnippets: true
            },
            bracketPairColorization: { enabled: true }
        });

        // Ctrl+Enter = submit
        editor.addAction({
            id: 'submit-code',
            label: 'Submit Code',
            keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter],
            run: function () { submitBtn.click(); }
        });

        // Cursor position in status bar
        editor.onDidChangeCursorPosition(function (e) {
            statusEl.textContent = 'Ln ' + e.position.lineNumber + ', Col ' + e.position.column;
        });

        statusEl.textContent = 'Ln 1, Col 1';
    });

    // Language switch
    langSelect.addEventListener('change', function () {
        var lang = langSelect.value;
        if (editor) {
            var model = editor.getModel();
            monaco.editor.setModelLanguage(model, LANG_MAP[lang]);
            editor.setValue(TEMPLATES[lang]);
        }
    });

    // Reset
    resetBtn.addEventListener('click', function () {
        if (editor) {
            editor.setValue(TEMPLATES[langSelect.value]);
            editor.focus();
        }
    });

    // WebSocket
    var ws = API.connectWs();

    ws.onopen = function () {
        ws.send(JSON.stringify({ action: 'join_room', room_id: match.room_id }));
    };

    ws.onmessage = function (e) {
        var msg = JSON.parse(e.data);

        if (msg.type === 'submission') {
            var el = playerStatus[msg.username];
            if (el) {
                el.textContent = msg.status;
                el.className = 'status status-' + msg.status;
            }
        }
        else if (msg.type === 'player_left') {
            var li = document.getElementById('player-' + msg.username);
            if (li) li.style.opacity = '0.4';
        }
    };

    // 5 min timer
    var remaining = 300;
    var tick = setInterval(function () {
        remaining--;
        var m = Math.floor(remaining / 60);
        var s = remaining % 60;
        timerEl.textContent = String(m).padStart(2, '0') + ':' + String(s).padStart(2, '0');

        if (remaining <= 60) timerEl.classList.add('danger');
        if (remaining <= 0) {
            clearInterval(tick);
            timerEl.textContent = "TIME'S UP";
        }
    }, 1000);

    // Submit
    submitBtn.addEventListener('click', function () {
        if (!editor) return;
        var code = editor.getValue().trim();
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
        setTimeout(function () {
            submitBtn.disabled = false;
            submitBtn.textContent = 'Submit';
        }, 3000);
    });

    // Leave
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
