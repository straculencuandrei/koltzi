// Koltzi Frontend Application Logic

document.addEventListener('DOMContentLoaded', () => {
    // 1. Initialize Living Ghost Companion
    const canvas = document.getElementById('ghost-canvas');
    const bubble = document.getElementById('ghost-bubble');
    const ghost = new GhostCompanion(canvas, bubble);

    let currentReport = null;
    let telemetryFilter = 'ALL';

    // 60 FPS Animation Loop
    let lastTime = performance.now();
    function animate(now) {
        const dt = Math.min(0.05, (now - lastTime) / 1000);
        lastTime = now;
        ghost.update(dt);
        ghost.render();
        requestAnimationFrame(animate);
    }
    requestAnimationFrame(animate);

    // 2. Custom Window Controls
    document.getElementById('btn-win-min').addEventListener('click', () => {
        if (window.koltzi) window.koltzi.minimize();
    });
    document.getElementById('btn-win-max').addEventListener('click', () => {
        if (window.koltzi) window.koltzi.maximize();
    });
    document.getElementById('btn-win-close').addEventListener('click', () => {
        if (window.koltzi) window.koltzi.close();
    });

    // 3. Tab Switching
    const tabBtns = document.querySelectorAll('.tab-btn');
    const tabPanes = document.querySelectorAll('.tab-pane');

    tabBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const targetTab = btn.getAttribute('data-tab');
            tabBtns.forEach(b => b.classList.remove('active'));
            tabPanes.forEach(p => p.classList.remove('active'));

            btn.classList.add('active');
            const targetPane = document.getElementById(`pane-${targetTab}`);
            if (targetPane) targetPane.classList.add('active');
        });
    });

    // 4. Sample Profile Presets
    const sampleBtns = document.querySelectorAll('[data-sample]');
    sampleBtns.forEach(btn => {
        btn.addEventListener('click', async () => {
            const sampleType = btn.getAttribute('data-sample');
            sampleBtns.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');

            ghost.setMood('sniffing');
            ghost.setDialogue(`Analyzing profile sample ${sampleType}. Parsing PE structures and scanning instruction streams...`, false);

            if (window.koltzi) {
                try {
                    const report = await window.koltzi.analyzeSample(sampleType);
                    renderReport(report);
                } catch (err) {
                    ghost.setMood('alarmed');
                    ghost.setDialogue(`Failed to analyze sample. ${err.message || err}`, true);
                }
            }
        });
    });

    // 5. File Picker
    const btnOpenBinary = document.getElementById('btn-open-binary');
    const dropZone = document.getElementById('drop-zone');

    async function triggerOpenFile() {
        if (!window.koltzi) return;
        const filePath = await window.koltzi.selectFile();
        if (filePath) {
            analyzeFile(filePath);
        }
    }

    btnOpenBinary.addEventListener('click', triggerOpenFile);
    dropZone.addEventListener('click', triggerOpenFile);

    // 6. Robust HTML5 Drag and Drop Support
    ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
        window.addEventListener(eventName, (e) => {
            e.preventDefault();
            e.stopPropagation();
        }, false);
        document.addEventListener(eventName, (e) => {
            e.preventDefault();
            e.stopPropagation();
        }, false);
    });

    ['dragenter', 'dragover'].forEach(eventName => {
        window.addEventListener(eventName, (e) => {
            e.dataTransfer.dropEffect = 'copy';
            dropZone.classList.add('drag-over');
        }, false);
    });

    ['dragleave', 'dragend'].forEach(eventName => {
        window.addEventListener(eventName, (e) => {
            if (e.clientX <= 0 || e.clientY <= 0 || e.relatedTarget === null) {
                dropZone.classList.remove('drag-over');
            }
        }, false);
    });

    window.addEventListener('drop', (e) => {
        dropZone.classList.remove('drag-over');

        const dt = e.dataTransfer;
        if (dt && dt.files && dt.files.length > 0) {
            const file = dt.files[0];
            let filePath = '';

            if (window.koltzi && typeof window.koltzi.getPathForFile === 'function') {
                try {
                    filePath = window.koltzi.getPathForFile(file);
                } catch (err) {
                    console.error('getPathForFile failed:', err);
                }
            }
            if (!filePath && file.path) {
                filePath = file.path;
            }

            if (filePath) {
                analyzeFile(filePath);
            } else {
                ghost.setMood('puzzled');
                ghost.setDialogue('Could not resolve file path for dropped binary.', true);
            }
        }
    }, false);

    // 7. Theme Switching Engine
    const themeBtns = document.querySelectorAll('[data-theme-btn]');
    function applyTheme(themeName) {
        document.body.setAttribute('data-theme', themeName);
        themeBtns.forEach(b => {
            if (b.getAttribute('data-theme-btn') === themeName) {
                b.classList.add('active');
            } else {
                b.classList.remove('active');
            }
        });
        ghost.setTheme(themeName);
        try {
            localStorage.setItem('koltzi_theme', themeName);
        } catch (e) {}
    }

    themeBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const theme = btn.getAttribute('data-theme-btn');
            applyTheme(theme);
        });
    });

    // Restore saved theme or default to ember
    try {
        const savedTheme = localStorage.getItem('koltzi_theme') || 'ember';
        applyTheme(savedTheme);
    } catch (e) {
        applyTheme('ember');
    }

    async function analyzeFile(filePath) {
        sampleBtns.forEach(b => b.classList.remove('active'));
        ghost.setMood('sniffing');
        ghost.setDialogue(`Ingesting target binary. Parsing PE headers, validating digital certificates, and scanning instruction streams.`, false);

        if (window.koltzi) {
            try {
                const report = await window.koltzi.analyzeFile(filePath);
                renderReport(report);
            } catch (err) {
                ghost.setMood('alarmed');
                ghost.setDialogue(`Failed to parse target binary. ${err.message || err}`, true);
            }
        }
    }

    // 7. Telemetry Log Subsystem Filter
    const filterBtns = document.querySelectorAll('.log-filter-btn');
    filterBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            filterBtns.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            telemetryFilter = btn.getAttribute('data-filter');
            renderTelemetryTable();
        });
    });

    // 8. Hash Tile Click-to-Copy
    document.querySelectorAll('.hash-tile').forEach(tile => {
        tile.addEventListener('click', () => {
            const valElem = tile.querySelector('.hash-value');
            if (valElem && valElem.textContent && valElem.textContent !== 'Pending ingestion') {
                navigator.clipboard.writeText(valElem.textContent);
                const orig = valElem.textContent;
                valElem.textContent = 'Copied to clipboard!';
                setTimeout(() => { valElem.textContent = orig; }, 1500);
            }
        });
    });

    // 9. Render Engine Report to UI
    function renderReport(report) {
        if (!report) return;
        currentReport = report;

        // Titlebar & Header
        document.getElementById('active-target-title').textContent = report.fileName || 'Unknown Binary';
        const archText = report.machineType ? `${report.machineType} (${report.is64Bit ? 'x64' : 'x86'})` : 'Native PE';
        document.getElementById('target-arch-pill').textContent = archText;

        // Threat Score
        animateNumber(document.getElementById('threat-score-num'), report.threatScore || 0);

        // Verdict Badge & Description
        const verdictBadge = document.getElementById('verdict-badge');
        const verdictDesc = document.getElementById('verdict-desc');
        const score = report.threatScore || 0;

        verdictBadge.className = 'verdict-badge';
        if (score >= 60) {
            verdictBadge.classList.add('threat');
            verdictBadge.textContent = 'Critical threat';
            verdictDesc.textContent = 'Malicious indicators detected. Direct syscalls or stealth injection loops identified.';
        } else if (score >= 20) {
            verdictBadge.classList.add('warn');
            verdictBadge.textContent = 'Suspicious';
            verdictDesc.textContent = 'Heuristic discrepancies found. High entropy packed code detected.';
        } else if (report.isInstaller) {
            verdictBadge.classList.add('pass');
            verdictBadge.textContent = 'Safe installer';
            verdictDesc.textContent = 'Verified installation archive signature.';
        } else {
            verdictBadge.classList.add('pass');
            verdictBadge.textContent = 'Clean / pass';
            verdictDesc.textContent = 'Zero threat flags detected. Headers and sections match standard Windows binaries.';
        }

        // Ghost Companion Mood & Personality Dialogue
        // Map report mood enum: 0 = Idle, 1 = Sniffing, 2 = Happy, 3 = Puzzled, 4 = Alarmed
        let ghostMood = 'idle';
        if (report.mood === 2) ghostMood = 'happy';
        else if (report.mood === 3) ghostMood = 'puzzled';
        else if (report.mood === 4) ghostMood = 'alarmed';
        else if (report.mood === 1) ghostMood = 'sniffing';

        ghost.setMood(ghostMood);
        if (report.personalityDialogue) {
            ghost.setDialogue(report.personalityDialogue, false);
        }

        // Telemetry Summary
        const statSig = document.getElementById('stat-sig');
        if (report.signature && report.signature.isValid) {
            statSig.textContent = `Verified (${report.signature.signerSubject || 'Trusted Vendor'})`;
            statSig.style.color = 'var(--status-pass)';
        } else if (report.signature && report.signature.isSigned) {
            statSig.textContent = 'Unverified / Self-signed certificate';
            statSig.style.color = 'var(--status-warn)';
        } else {
            statSig.textContent = 'Unsigned / Missing Authenticode';
            statSig.style.color = 'var(--text-muted)';
        }

        // Entropy
        const entropy = report.overallEntropy || 0;
        const isHighEnt = entropy > 7.2;
        const statEnt = document.getElementById('stat-entropy');
        statEnt.textContent = `${entropy.toFixed(2)} / 8.00  (${isHighEnt ? 'Packed / High' : 'Normal'})`;
        statEnt.style.color = isHighEnt ? 'var(--status-warn)' : 'var(--accent-cyan)';

        const entFill = document.getElementById('entropy-progress-fill');
        const entPct = Math.min(100, (entropy / 8.0) * 100);
        entFill.style.width = `${entPct}%`;
        entFill.style.backgroundColor = isHighEnt ? 'var(--status-warn)' : 'var(--accent-cyan)';

        document.getElementById('stat-arch').textContent = `${report.machineType || 'x64'} • ${report.subsystem || 'Windows GUI'}`;
        document.getElementById('stat-imports').textContent = `${(report.imports || []).length} resolved dependencies`;

        const lat = report.analysisTimeMs ? report.analysisTimeMs.toFixed(1) : '0.8';
        document.getElementById('stat-latency').textContent = `${lat} ms (Air-gapped offline static pass)`;

        // Overview Tab: Findings List
        renderFindings(report);

        // Hashes
        document.getElementById('hash-sha256').textContent = report.sha256 || 'N/A';
        document.getElementById('hash-imphash').textContent = report.imphash || 'N/A';
        document.getElementById('hash-md5').textContent = report.md5 || 'N/A';
        document.getElementById('hash-sha1').textContent = report.sha1 || 'N/A';

        // Sections Heatmap & Table
        renderSections(report);

        // Telemetry Logs Table
        const logCount = (report.logEntries || []).length;
        document.getElementById('tab-telemetry-btn').textContent = `Telemetry log (${logCount})`;
        renderTelemetryTable();
    }

    function renderFindings(report) {
        const container = document.getElementById('findings-container');
        container.innerHTML = '';

        const details = report.technicalDetails || [];
        document.getElementById('findings-count-badge').textContent = `${details.length} findings`;

        if (details.length === 0) {
            container.innerHTML = `
                <div class="finding-row">
                    <span class="finding-pill info">INFO</span>
                    <span class="finding-text">No static threat indicators detected. Standard binary execution flow.</span>
                </div>
            `;
            return;
        }

        details.forEach(item => {
            let tag = 'INFO';
            let pillClass = 'info';
            let text = item;

            if (item.startsWith('[CRITICAL]')) {
                tag = 'CRITICAL';
                pillClass = 'critical';
                text = item.substring(10).trim();
            } else if (item.startsWith('[WARNING]')) {
                tag = 'WARNING';
                pillClass = 'warning';
                text = item.substring(9).trim();
            } else if (item.startsWith('[INFO]')) {
                tag = 'INFO';
                pillClass = 'info';
                text = item.substring(6).trim();
            }

            const row = document.createElement('div');
            row.className = 'finding-row';
            row.innerHTML = `
                <span class="finding-pill ${pillClass}">${tag}</span>
                <span class="finding-text">${escapeHtml(text)}</span>
            `;
            container.appendChild(row);
        });
    }

    function renderSections(report) {
        const heatmap = document.getElementById('sections-heatmap');
        const tbody = document.getElementById('sections-table-body');
        heatmap.innerHTML = '';
        tbody.innerHTML = '';

        const sections = report.sections || [];
        if (sections.length === 0) {
            heatmap.innerHTML = '<div class="section-slice" style="width: 100%; background: rgba(255,255,255,0.06);">No sections loaded</div>';
            tbody.innerHTML = '<tr><td colspan="5" style="color: var(--text-dim); text-align: center; padding: 24px;">No sections loaded.</td></tr>';
            return;
        }

        let totalVirt = sections.reduce((sum, s) => sum + Math.max(1, s.virtualSize || 0), 0);
        if (totalVirt === 0) totalVirt = 1;

        sections.forEach(s => {
            const pct = Math.max(8, (s.virtualSize / totalVirt) * 100);
            const isPacked = (s.entropy || 0) > 7.2;
            const isMedium = (s.entropy || 0) >= 6.0;

            const sliceColor = isPacked ? 'var(--status-threat)' : isMedium ? 'var(--accent-cyan)' : 'var(--status-pass)';
            const slice = document.createElement('div');
            slice.className = 'section-slice';
            slice.style.width = `${pct}%`;
            slice.style.backgroundColor = sliceColor;
            slice.textContent = `${s.name} (${(s.entropy || 0).toFixed(1)})`;
            slice.title = `${s.name}: Entropy ${(s.entropy || 0).toFixed(2)}, Size ${formatBytes(s.virtualSize)}`;
            heatmap.appendChild(slice);

            // Table Row
            const tr = document.createElement('tr');
            let permVerdict = s.isExecutable ? 'Executable code [R-X]' : 'Data segment [R--]';
            let permColor = 'var(--text-secondary)';

            if (s.isRwx) {
                permVerdict = '[CRIT] RWX executable and writable';
                permColor = 'var(--status-threat)';
            } else if (s.isSuspiciousEntropy) {
                permVerdict = report.isInstaller ? '[PASS] Compressed archive' : '[WARN] High entropy / packed';
                permColor = report.isInstaller ? 'var(--accent-cyan)' : 'var(--status-warn)';
            }

            tr.innerHTML = `
                <td style="font-weight: 600; color: var(--text-primary);">${escapeHtml(s.name)}</td>
                <td>0x${(s.virtualAddress || 0).toString(16).toUpperCase().padStart(8, '0')}</td>
                <td>0x${(s.virtualSize || 0).toString(16).toUpperCase()} (${formatBytes(s.virtualSize)})</td>
                <td style="color: ${sliceColor}; font-weight: 500;">${(s.entropy || 0).toFixed(2)}</td>
                <td style="color: ${permColor};">${escapeHtml(permVerdict)}</td>
            `;
            tbody.appendChild(tr);
        });
    }

    function renderTelemetryTable() {
        const tbody = document.getElementById('telemetry-table-body');
        tbody.innerHTML = '';

        if (!currentReport || !currentReport.logEntries || currentReport.logEntries.length === 0) {
            tbody.innerHTML = '<tr><td colspan="4" style="color: var(--text-dim); text-align: center; padding: 24px;">No telemetry records logged. Standby for binary ingestion.</td></tr>';
            return;
        }

        const filtered = currentReport.logEntries.filter(entry => {
            if (telemetryFilter === 'ALL') return true;
            return entry.subsystem === telemetryFilter;
        });

        if (filtered.length === 0) {
            tbody.innerHTML = `<tr><td colspan="4" style="color: var(--text-dim); text-align: center; padding: 24px;">No records match filter: ${telemetryFilter}</td></tr>`;
            return;
        }

        filtered.forEach(entry => {
            const tr = document.createElement('tr');
            tr.className = 'telemetry-row';
            tr.innerHTML = `
                <td style="color: var(--text-dim);">${escapeHtml(entry.timestamp)}</td>
                <td class="telemetry-subsys-tag">[${escapeHtml(entry.subsystem)}]</td>
                <td><span class="telemetry-badge ${escapeHtml(entry.level)}">${escapeHtml(entry.level)}</span></td>
                <td style="color: var(--text-primary);">${escapeHtml(entry.message)}</td>
            `;
            tbody.appendChild(tr);
        });
    }

    function animateNumber(element, target) {
        const start = parseInt(element.textContent, 10) || 0;
        const duration = 450;
        const startTime = performance.now();

        function update(now) {
            const elapsed = now - startTime;
            const progress = Math.min(1, elapsed / duration);
            const val = Math.round(start + (target - start) * (1 - Math.pow(1 - progress, 3)));
            element.textContent = val;
            if (progress < 1) requestAnimationFrame(update);
        }
        requestAnimationFrame(update);
    }

    function formatBytes(bytes) {
        if (!bytes || bytes === 0) return '0 B';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    function escapeHtml(str) {
        if (!str) return '';
        return String(str).replace(/[&<>"']/g, s => ({
            '&': '&amp;',
            '<': '&lt;',
            '>': '&gt;',
            '"': '&quot;',
            "'": '&#39;'
        }[s]));
    }

    // Auto-load clean sample on initial launch
    setTimeout(() => {
        const cleanBtn = document.querySelector('[data-sample="clean"]');
        if (cleanBtn) cleanBtn.click();
    }, 400);
});
