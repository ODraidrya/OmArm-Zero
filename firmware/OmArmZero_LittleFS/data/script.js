// DOM Elements
const statusDot = document.getElementById('status-dot');
const statusText = document.getElementById('status-text');
const homeBtn = document.getElementById('home-btn');

// Sliders
const sliders = [
    document.getElementById('j1'),
    document.getElementById('j2'),
    document.getElementById('j3'),
    document.getElementById('j4'),
    document.getElementById('j5'),
    document.getElementById('j6')
];

const valueDisplays = [
    document.getElementById('val-j1'),
    document.getElementById('val-j2'),
    document.getElementById('val-j3'),
    document.getElementById('val-j4'),
    document.getElementById('val-j5'),
    document.getElementById('val-j6')
];

// Sequence Controls
const addPoseBtn = document.getElementById('add-pose-btn');
const playBtn = document.getElementById('play-btn');
const stopBtn = document.getElementById('stop-btn');
const sequenceList = document.getElementById('sequence-list');
const playDelayInput = document.getElementById('play-delay');
const playDelayVal = document.getElementById('delay-val');

playDelayInput.addEventListener('input', (e) => {
    playDelayVal.textContent = e.target.value + ' ms';
});

const exportBtn = document.getElementById('export-btn');
const importBtn = document.getElementById('import-btn');
const fileInput = document.getElementById('file-input');

// State
let sequence = [];
let isPlaying = false;
let currentPoseIndex = -1;

// ------------------------------------------------------------------
// Auto-connect / Status Check
// ------------------------------------------------------------------
setInterval(() => {
    fetch('/api/command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ cmd: 'STATUS' })
    })
        .then(res => {
            if (res.ok) {
                statusDot.classList.add('connected');
                statusText.textContent = 'Connected';
            } else {
                throw new Error();
            }
        })
        .catch(() => {
            statusDot.classList.remove('connected');
            statusText.textContent = 'Disconnected';
        });
}, 2000);

// ------------------------------------------------------------------
// Sliders
// ------------------------------------------------------------------
sliders.forEach((slider, index) => {
    slider.addEventListener('input', (e) => {
        const val = e.target.value;
        valueDisplays[index].textContent = val + '°';
        sendCommand(`${index + 1},${val}`);
    });
});

homeBtn.addEventListener('click', () => {
    sendCommand('HOME');
    sliders.forEach((s, i) => {
        s.value = 0;
        valueDisplays[i].textContent = '0°';
    });
});

// Speed Control
const speedSlider = document.getElementById('motor-speed');
const speedVal = document.getElementById('speed-val');

speedSlider.addEventListener('input', (e) => {
    speedVal.textContent = e.target.value + '%';
});

speedSlider.addEventListener('change', (e) => {
    const val = parseInt(e.target.value);
    let step, period;

    if (val < 50) {
        const t = val / 50;
        step = Math.max(1, Math.floor(1 + t * 4));
        period = Math.floor(40 - t * 25);
    } else {
        const t = (val - 50) / 50;
        step = Math.floor(5 + t * 15);
        period = Math.max(5, Math.floor(15 - t * 10));
    }

    sendCommand(`SPEED:${step},${period}`);
});

// ------------------------------------------------------------------
// Sequence Logic
// ------------------------------------------------------------------

addPoseBtn.addEventListener('click', () => {
    const pose = sliders.map(s => parseInt(s.value));
    sequence.push(pose);
    renderSequence();
    playBtn.disabled = false;
});

function renderSequence() {
    sequenceList.innerHTML = '';

    if (sequence.length === 0) {
        sequenceList.innerHTML = '<div class="empty-state">No poses added yet</div>';
        playBtn.disabled = true;
        return;
    }

    sequence.forEach((pose, index) => {
        const item = document.createElement('div');
        item.className = `pose-item ${index === currentPoseIndex ? 'active' : ''}`;

        item.innerHTML = `
            <div class="pose-info">
                <div class="pose-name">Pose #${index + 1}</div>
                <div class="pose-details">J1:${pose[0]} J2:${pose[1]} J3:${pose[2]} J4:${pose[3]} J5:${pose[4]} J6:${pose[5]}</div>
            </div>
            <button class="pose-delete" onclick="deletePose(${index})">
                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>
            </button>
        `;
        sequenceList.appendChild(item);
    });
}

window.deletePose = (index) => {
    sequence.splice(index, 1);
    renderSequence();
    if (sequence.length === 0) playBtn.disabled = true;
};

playBtn.addEventListener('click', async () => {
    if (isPlaying) return;
    isPlaying = true;
    playBtn.disabled = true;
    stopBtn.disabled = false;
    addPoseBtn.disabled = true;

    const delay = parseInt(playDelayInput.value) || 1000;

    while (isPlaying) {
        for (let i = 0; i < sequence.length; i++) {
            if (!isPlaying) break;

            currentPoseIndex = i;
            renderSequence();

            const pose = sequence[i];
            const cmd = `MOVE:${pose.join(',')}`;
            await sendCommand(cmd);

            pose.forEach((val, idx) => {
                sliders[idx].value = val;
                valueDisplays[idx].textContent = val + '°';
            });

            await new Promise(r => setTimeout(r, delay));
        }
    }

    currentPoseIndex = -1;
    renderSequence();
    playBtn.disabled = false;
    stopBtn.disabled = true;
    addPoseBtn.disabled = false;
});

stopBtn.addEventListener('click', () => {
    isPlaying = false;
});

// ------------------------------------------------------------------
// Import / Export
// ------------------------------------------------------------------

exportBtn.addEventListener('click', () => {
    if (sequence.length === 0) {
        alert('No sequence to export');
        return;
    }
    const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(sequence));
    const downloadAnchorNode = document.createElement('a');
    downloadAnchorNode.setAttribute("href", dataStr);
    downloadAnchorNode.setAttribute("download", "omarm_sequence.json");
    document.body.appendChild(downloadAnchorNode);
    downloadAnchorNode.click();
    downloadAnchorNode.remove();
});

importBtn.addEventListener('click', () => {
    fileInput.click();
});

fileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (event) => {
        try {
            const imported = JSON.parse(event.target.result);
            if (Array.isArray(imported) && imported.every(p => Array.isArray(p) && p.length === 6)) {
                sequence = imported;
                renderSequence();
                playBtn.disabled = false;
                alert('Sequence imported successfully');
            } else {
                alert('Invalid file format. Expected array of 6-value arrays.');
            }
        } catch (err) {
            alert('Error parsing JSON: ' + err.message);
        }
    };
    reader.readAsText(file);
    fileInput.value = '';
});

// ------------------------------------------------------------------
// API Communication
// ------------------------------------------------------------------

async function sendCommand(cmd) {
    try {
        await fetch('/api/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ cmd: cmd })
        });
    } catch (err) {
        console.error('Send error:', err);
    }
}

// ------------------------------------------------------------------
// Visual Interface (Lines)
// ------------------------------------------------------------------

function updateLines() {
    const svg = document.getElementById('lines-svg');
    svg.innerHTML = '';

    const markers = [
        document.getElementById('m-j1'),
        document.getElementById('m-j2'),
        document.getElementById('m-j3'),
        document.getElementById('m-j4'),
        document.getElementById('m-j5'),
        document.getElementById('m-j6')
    ];

    const groups = [
        document.getElementById('grp-j1'),
        document.getElementById('grp-j2'),
        document.getElementById('grp-j3'),
        document.getElementById('grp-j4'),
        document.getElementById('grp-j5'),
        document.getElementById('grp-j6')
    ];

    markers.forEach((marker, index) => {
        const group = groups[index];
        if (!marker || !group) return;

        const mRect = marker.getBoundingClientRect();
        const gRect = group.getBoundingClientRect();
        const svgRect = svg.getBoundingClientRect();

        const x1 = mRect.left + mRect.width / 2 - svgRect.left;
        const y1 = mRect.top + mRect.height / 2 - svgRect.top;
        const x2 = gRect.left - svgRect.left;
        const y2 = gRect.top + gRect.height / 2 - svgRect.top;

        const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        const cp1x = x1 + (x2 - x1) * 0.5;
        const cp1y = y1;
        const cp2x = x2 - (x2 - x1) * 0.5;
        const cp2y = y2;
        const d = `M ${x1} ${y1} C ${cp1x} ${cp1y}, ${cp2x} ${cp2y}, ${x2} ${y2}`;

        path.setAttribute('d', d);
        path.setAttribute('stroke', '#3b82f6');
        path.setAttribute('stroke-width', '2');
        path.setAttribute('fill', 'none');
        path.setAttribute('stroke-opacity', '0.6');

        svg.appendChild(path);
    });
}

window.addEventListener('resize', updateLines);
window.addEventListener('load', updateLines);

const slidersColumn = document.querySelector('.sliders-column');
if (slidersColumn) {
    slidersColumn.addEventListener('scroll', () => {
        requestAnimationFrame(updateLines);
    });
}
setInterval(updateLines, 1000);
