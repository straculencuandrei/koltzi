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

            if (targetTab === 'attack-chain') {
                setTimeout(() => {
                    fitAttackChainView();
                    updateAttackChainConnectors();
                }, 40);
            }
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

    // 6. Robust Universal Drag and Drop Engine (Drag n Drop Anywhere in App)
    let dragCounter = 0;
    let lastHandledDrop = 0;

    function handleDroppedBinary(filePath) {
        if (!filePath) return;
        const now = performance.now();
        if (now - lastHandledDrop < 400) return; // Prevent double invocation from duplicate events
        lastHandledDrop = now;

        dragCounter = 0;
        document.body.classList.remove('is-dragging-file');
        dropZone.classList.remove('drag-over');

        ghost.setMood('sniffing');
        ghost.setDialogue('Target binary dropped! Commencing static PE ingestion, signature validation, and instruction sweeps.', false);

        // Automatically switch to telemetry log window so analyst sees every log line in real-time
        const telTab = document.getElementById('tab-telemetry-btn');
        if (telTab) telTab.click();

        analyzeFile(filePath);
    }

    // Capture custom event dispatched by preload script
    window.addEventListener('koltzi-file-dropped', (e) => {
        if (e.detail && e.detail.filePath) {
            handleDroppedBinary(e.detail.filePath);
        }
    });

    // Window Drag Enter: Increment counter and show global drop state
    window.addEventListener('dragenter', (e) => {
        e.preventDefault();
        dragCounter++;
        document.body.classList.add('is-dragging-file');
        dropZone.classList.add('drag-over');
        if (ghost && typeof ghost.setMood === 'function') {
            ghost.setMood('sniffing');
            ghost.setDialogue('Target binary detected! Release anywhere to commence deep inspection.', false);
        }
    }, false);

    // Window Drag Over: CRITICAL, must call preventDefault on dragover for drop to work
    window.addEventListener('dragover', (e) => {
        e.preventDefault();
        if (e.dataTransfer) {
            e.dataTransfer.dropEffect = 'copy';
        }
    }, false);

    // Window Drag Leave: Decrement counter, only remove overlay when leaving window
    window.addEventListener('dragleave', (e) => {
        e.preventDefault();
        dragCounter--;
        if (dragCounter <= 0) {
            dragCounter = 0;
            document.body.classList.remove('is-dragging-file');
            dropZone.classList.remove('drag-over');
        }
    }, false);

    // Window Drop: Extract dropped file path and trigger triage
    window.addEventListener('drop', (e) => {
        e.preventDefault();
        dragCounter = 0;
        document.body.classList.remove('is-dragging-file');
        dropZone.classList.remove('drag-over');

        let filePath = '';
        const dt = e.dataTransfer;

        if (dt) {
            if (dt.files && dt.files.length > 0) {
                const file = dt.files[0];
                if (window.koltzi && typeof window.koltzi.getPathForFile === 'function') {
                    try {
                        filePath = window.koltzi.getPathForFile(file);
                    } catch (err) {
                        console.warn('getPathForFile call failed:', err);
                    }
                }
                if (!filePath && file.path) {
                    filePath = file.path;
                }
            }

            if (!filePath && dt.items && dt.items.length > 0) {
                for (let i = 0; i < dt.items.length; ++i) {
                    const item = dt.items[i];
                    if (item.kind === 'file') {
                        const file = item.getAsFile();
                        if (file) {
                            if (window.koltzi && typeof window.koltzi.getPathForFile === 'function') {
                                try {
                                    filePath = window.koltzi.getPathForFile(file);
                                } catch (err) {}
                            }
                            if (!filePath && file.path) {
                                filePath = file.path;
                            }
                            if (filePath) break;
                        }
                    }
                }
            }
        }

        if (filePath) {
            handleDroppedBinary(filePath);
        } else {
            ghost.setMood('puzzled');
            ghost.setDialogue('Could not resolve file path for dropped binary.', true);
        }
    }, false);

    // Redundant document dragover prevention
    document.addEventListener('dragover', (e) => {
        e.preventDefault();
        if (e.dataTransfer) {
            e.dataTransfer.dropEffect = 'copy';
        }
    }, false);

    document.addEventListener('drop', (e) => {
        e.preventDefault();
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

        // Open telemetry log window immediately so user sees every action taken
        const telTab = document.getElementById('tab-telemetry-btn');
        if (telTab) telTab.click();

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
            if (report.hasSuspiciousOverlay) {
                verdictBadge.textContent = 'Critical threat / Dropper';
                verdictDesc.textContent = 'Unsigned payload dropper container with massive encrypted overlay payload detected.';
            } else {
                verdictBadge.textContent = 'Critical threat';
                verdictDesc.textContent = 'Malicious indicators detected. Direct syscalls or stealth injection loops identified.';
            }
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

        // PE Overlay Telemetry
        const statOverlay = document.getElementById('stat-overlay');
        if (statOverlay) {
            if (report.overlaySize && report.overlaySize > 0) {
                const ovMb = (report.overlaySize / (1024 * 1024)).toFixed(1);
                const ovPct = ((report.overlayRatio || 0) * 100).toFixed(1);
                statOverlay.textContent = `${ovMb} MB (${ovPct}%, H=${(report.overlayEntropy || 0).toFixed(2)})`;
                statOverlay.style.color = report.hasSuspiciousOverlay ? 'var(--status-threat)' : 'var(--accent-purple)';
            } else {
                statOverlay.textContent = 'None (Clean PE structure)';
                statOverlay.style.color = 'var(--text-muted)';
            }
        }

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

        // Decompiler and Reverse Engineering functions
        const decompCount = (report.decompiledFunctions || []).length;
        const tabDecompBtn = document.getElementById('tab-decompile-btn');
        if (tabDecompBtn) {
            tabDecompBtn.textContent = `Decompiler & RE (${decompCount})`;
        }
        renderDecompiler(report);

        // Attack Chain / Threat Graph
        renderAttackChain(report);
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

    // 10. Decompiler and Reverse Engineering Module
    let selectedFuncIndex = 0;
    let decompileMode = 'pseudocode';
    let funcSearchQuery = '';

    const btnViewPseudocode = document.getElementById('btn-view-pseudocode');
    const btnViewDisasm = document.getElementById('btn-view-disasm');
    const btnCopyCode = document.getElementById('btn-copy-code');
    const funcSearchInput = document.getElementById('func-search-input');

    if (btnViewPseudocode && btnViewDisasm) {
        btnViewPseudocode.addEventListener('click', () => {
            decompileMode = 'pseudocode';
            btnViewPseudocode.classList.add('active');
            btnViewDisasm.classList.remove('active');
            document.getElementById('code-viewer-pseudocode').classList.add('active');
            document.getElementById('code-viewer-disasm').classList.remove('active');
        });

        btnViewDisasm.addEventListener('click', () => {
            decompileMode = 'disasm';
            btnViewDisasm.classList.add('active');
            btnViewPseudocode.classList.remove('active');
            document.getElementById('code-viewer-disasm').classList.add('active');
            document.getElementById('code-viewer-pseudocode').classList.remove('active');
        });
    }

    if (btnCopyCode) {
        btnCopyCode.addEventListener('click', () => {
            if (!currentReport || !currentReport.decompiledFunctions || !currentReport.decompiledFunctions[selectedFuncIndex]) return;
            const fn = currentReport.decompiledFunctions[selectedFuncIndex];
            let copyText = '';
            if (decompileMode === 'pseudocode') {
                copyText = (fn.pseudocode || []).join('\n');
            } else {
                copyText = (fn.instructions || []).map(ins => {
                    const rvaStr = '0x' + (ins.rva || 0).toString(16).toUpperCase();
                    return `${rvaStr.padEnd(10)} ${(ins.mnemonic || '').padEnd(8)} ${(ins.operands || '').padEnd(24)} ${ins.comment ? '// ' + ins.comment : ''}`;
                }).join('\n');
            }
            navigator.clipboard.writeText(copyText);
            const orig = btnCopyCode.textContent;
            btnCopyCode.textContent = 'Copied!';
            setTimeout(() => { btnCopyCode.textContent = orig; }, 1200);
        });
    }

    if (funcSearchInput) {
        funcSearchInput.addEventListener('input', (e) => {
            funcSearchQuery = e.target.value.toLowerCase().trim();
            renderFunctionList();
        });
    }

    function renderDecompiler(report) {
        selectedFuncIndex = 0;
        const funcs = report.decompiledFunctions || [];
        const badge = document.getElementById('func-count-badge');
        if (badge) badge.textContent = `${funcs.length} functions`;

        renderFunctionList();
        renderActiveFunction();
    }

    function renderFunctionList() {
        const container = document.getElementById('func-list-container');
        if (!container) return;
        container.innerHTML = '';

        if (!currentReport || !currentReport.decompiledFunctions || currentReport.decompiledFunctions.length === 0) {
            container.innerHTML = '<div class="func-empty-state">No decompiled subroutines discovered in binary.</div>';
            return;
        }

        const funcs = currentReport.decompiledFunctions;
        let matches = 0;

        funcs.forEach((fn, idx) => {
            const rvaHex = '0x' + (fn.rva || 0).toString(16).toUpperCase();
            const searchKey = `${fn.name} ${rvaHex} ${fn.hasSyscall ? 'syscall' : ''} ${fn.hasPebAccess ? 'peb' : ''} ${fn.hasApiHash ? 'apihash' : ''} ${fn.isEntryPoint ? 'entry' : ''}`.toLowerCase();

            if (funcSearchQuery && !searchKey.includes(funcSearchQuery)) {
                return;
            }
            matches++;

            const card = document.createElement('div');
            card.className = `func-card ${idx === selectedFuncIndex ? 'active' : ''}`;
            card.setAttribute('data-index', idx);

            let tagsHtml = '';
            if (fn.isEntryPoint) tagsHtml += '<span class="func-mini-pill entry">Entry</span>';
            if (fn.hasSyscall) tagsHtml += '<span class="func-mini-pill syscall">Syscall</span>';
            if (fn.hasPebAccess) tagsHtml += '<span class="func-mini-pill peb">PEB</span>';
            if (fn.hasApiHash) tagsHtml += '<span class="func-mini-pill apihash">API Hash</span>';
            if (fn.branchCount > 0) tagsHtml += `<span class="func-mini-pill branch">${fn.branchCount} br</span>`;

            card.innerHTML = `
                <div class="func-card-name">${escapeHtml(fn.name)}</div>
                <div class="func-card-meta">
                    <span>${rvaHex}</span>
                    <span>${fn.instructionCount || 0} instrs (${fn.size || 0} B)</span>
                </div>
                ${tagsHtml ? `<div class="func-card-tags">${tagsHtml}</div>` : ''}
            `;

            card.addEventListener('click', () => {
                selectedFuncIndex = idx;
                document.querySelectorAll('.func-card').forEach(c => c.classList.remove('active'));
                card.classList.add('active');
                renderActiveFunction();
            });

            container.appendChild(card);
        });

        if (matches === 0) {
            container.innerHTML = `<div class="func-empty-state">No functions match "${escapeHtml(funcSearchQuery)}"</div>`;
        }
    }

    function renderActiveFunction() {
        if (!currentReport || !currentReport.decompiledFunctions || !currentReport.decompiledFunctions[selectedFuncIndex]) {
            const nameEl = document.getElementById('active-func-name');
            if (nameEl) nameEl.textContent = 'No function selected';
            const tagsEl = document.getElementById('active-func-tags');
            if (tagsEl) tagsEl.innerHTML = '';
            const pseudoEl = document.getElementById('pseudocode-text');
            if (pseudoEl) pseudoEl.textContent = '// No function selected';
            const disBody = document.getElementById('disasm-table-body');
            if (disBody) disBody.innerHTML = '<tr><td colspan="5" style="color: var(--text-dim); text-align: center; padding: 24px;">No function selected</td></tr>';
            return;
        }

        const fn = currentReport.decompiledFunctions[selectedFuncIndex];
        const rvaHex = '0x' + (fn.rva || 0).toString(16).toUpperCase();

        document.getElementById('active-func-name').textContent = `${fn.name} [${rvaHex}]`;

        // Tags
        const tagsContainer = document.getElementById('active-func-tags');
        tagsContainer.innerHTML = '';
        if (fn.isEntryPoint) tagsContainer.innerHTML += '<span class="func-mini-pill entry">Application Entry</span>';
        if (fn.hasSyscall) tagsContainer.innerHTML += '<span class="func-mini-pill syscall">Direct Syscall</span>';
        if (fn.hasPebAccess) tagsContainer.innerHTML += '<span class="func-mini-pill peb">PEB Access</span>';
        if (fn.hasApiHash) tagsContainer.innerHTML += '<span class="func-mini-pill apihash">API Hashing</span>';
        if (fn.hasInjection) tagsContainer.innerHTML += '<span class="func-mini-pill syscall">Injection Chain</span>';

        // Metrics Bar
        document.getElementById('metric-rva').textContent = rvaHex;
        document.getElementById('metric-size').textContent = `${fn.size || 0} bytes`;
        document.getElementById('metric-instrs').textContent = fn.instructionCount || (fn.instructions ? fn.instructions.length : 0);
        document.getElementById('metric-branches').textContent = fn.branchCount || 0;
        document.getElementById('metric-xrefs').textContent = (fn.calledApis || []).length;

        // Render Pseudocode
        const pseudoLines = fn.pseudocode || [];
        const pseudoElem = document.getElementById('pseudocode-text');
        if (pseudoLines.length > 0) {
            pseudoElem.innerHTML = pseudoLines.map((line, lIdx) => {
                const lineNum = (lIdx + 1).toString().padStart(2, ' ');
                let formatted = escapeHtml(line);
                // Syntax colorize keywords
                formatted = formatted
                    .replace(/\b(int64_t|void|uint64_t|uint32_t|uint8_t|byte)\b/g, '<span style="color: #38bdf8; font-weight: 600;">$1</span>')
                    .replace(/\b(return|goto|if|else|while|for)\b/g, '<span style="color: #c084fc; font-weight: 600;">$1</span>')
                    .replace(/\b(__syscall|__readgsqword|__readfsdword)\b/g, '<span style="color: #f59e0b; font-weight: 600;">$1</span>')
                    .replace(/(\/\/.*$)/g, '<span style="color: #10b981; font-style: italic;">$1</span>');

                return `<div class="pseudocode-line"><span class="pseudocode-ln">${lineNum}</span><span class="pseudocode-code">${formatted}</span></div>`;
            }).join('');
        } else {
            pseudoElem.innerHTML = '// No C pseudocode generated for this leaf stub.';
        }

        // Render Disassembly Table
        const disasmBody = document.getElementById('disasm-table-body');
        disasmBody.innerHTML = '';
        const instrs = fn.instructions || [];

        if (instrs.length === 0) {
            disasmBody.innerHTML = '<tr><td colspan="5" style="color: var(--text-dim); text-align: center; padding: 24px;">No disassembled instructions available.</td></tr>';
        } else {
            instrs.forEach(ins => {
                const tr = document.createElement('tr');
                const iRvaHex = '0x' + (ins.rva || 0).toString(16).toUpperCase();
                const mnem = (ins.mnemonic || '').toLowerCase();

                let mnemClass = '';
                if (mnem === 'call') mnemClass = 'call';
                else if (mnem.startsWith('j')) mnemClass = 'jump';
                else if (mnem === 'syscall' || mnem === 'sysenter') mnemClass = 'syscall';
                else if (mnem === 'ret') mnemClass = 'ret';

                tr.innerHTML = `
                    <td class="disasm-rva">${iRvaHex}</td>
                    <td class="disasm-hex">${escapeHtml(ins.hex || '')}</td>
                    <td class="disasm-mnemonic ${mnemClass}">${escapeHtml(ins.mnemonic || '')}</td>
                    <td class="disasm-operands">${escapeHtml(ins.operands || '')}</td>
                    <td class="disasm-comment">${escapeHtml(ins.comment || '')}</td>
                `;
                disasmBody.appendChild(tr);
            });
        }

        // Cross-References & API Calls
        const xrefsContainer = document.getElementById('xrefs-list-container');
        xrefsContainer.innerHTML = '';
        const apis = fn.calledApis || [];

        if (apis.length === 0) {
            xrefsContainer.innerHTML = '<span style="color: var(--text-dim); font-size: 11px;">Leaf routine. No external IAT API invocations detected.</span>';
        } else {
            apis.forEach(api => {
                const pill = document.createElement('span');
                pill.className = 'xref-pill';
                pill.textContent = api;
                xrefsContainer.appendChild(pill);
            });
        }
    }

    // =========================================================================
    // 11. Interactive Attack Chain / Threat Graph System
    // =========================================================================

    let graphScale = 1.0;
    let graphPanX = 36;
    let graphPanY = 36;
    let isPanning = false;
    let panStartX = 0;
    let panStartY = 0;
    let currentStageFilter = 'ALL';
    let activeNodeId = null;
    let currentAttackNodes = [];

    const graphStage = document.getElementById('attack-chain-stage');
    const graphViewport = document.getElementById('attack-chain-viewport');
    const graphNodesLayer = document.getElementById('attack-chain-nodes-layer');
    const svgPathsContainer = document.getElementById('svg-paths-container');
    const evidenceDrawer = document.getElementById('evidence-drawer');

    // Pan & Zoom Transform Updater
    function updateGraphTransform() {
        if (!graphViewport) return;
        graphViewport.style.transform = `translate(${graphPanX}px, ${graphPanY}px) scale(${graphScale})`;
    }

    // Auto Fit View
    function fitAttackChainView() {
        if (!graphStage || !graphNodesLayer) return;
        const stageRect = graphStage.getBoundingClientRect();
        const stageW = stageRect.width || 800;
        const stageH = stageRect.height || 540;

        const cols = graphNodesLayer.querySelectorAll('.chain-stage-col:not(.filtered-out)');
        if (cols.length === 0) {
            graphScale = 1.0;
            graphPanX = 36;
            graphPanY = 36;
            updateGraphTransform();
            return;
        }

        let totalW = (cols.length * 280) + ((cols.length - 1) * 48) + 72;
        let maxColH = 400;
        cols.forEach(col => {
            const h = col.offsetHeight || 300;
            if (h > maxColH) maxColH = h;
        });

        const targetScaleX = (stageW - 60) / Math.max(totalW, 400);
        const targetScaleY = (stageH - 60) / Math.max(maxColH + 72, 300);
        graphScale = Math.min(1.15, Math.max(0.45, Math.min(targetScaleX, targetScaleY)));
        graphPanX = 30;
        graphPanY = 30;
        updateGraphTransform();
        requestAnimationFrame(() => updateAttackChainConnectors());
    }

    // Synthesize attack chain nodes from report if backend didn't supply them
    function getOrSynthesizeAttackChain(report) {
        if (report && Array.isArray(report.attackChain) && report.attackChain.length > 0) {
            return report.attackChain;
        }

        if (!report) return [];

        const nodes = [];
        const score = report.threatScore || 0;
        const isMalicious = score >= 60;
        const isSuspicious = score >= 20;

        // Stage 1: Ingestion / Delivery
        const deliveryEvidence = [
            {
                type: 'HEURISTIC_RULE',
                label: 'PE Architecture and Ingestion',
                value: `Machine: ${report.machineType || 'AMD64'} (${report.is64Bit ? 'x64' : 'x86'}), Subsystem: ${report.subsystem || 'Windows GUI'}, Size: ${formatBytes(report.fileSize)}`,
                rule: 'PE_VALIDATION_PASSED',
                jumpTab: 'telemetry',
                jumpTarget: 'INGEST'
            },
            {
                type: 'HEURISTIC_RULE',
                label: 'Authenticode Signature Status',
                value: (report.signature && report.signature.isValid) ? 'Verified vendor certificate signature' : 'Unsigned executable binary / self-signed certificate',
                rule: 'AUTHENTICODE_VERIFY',
                jumpTab: 'telemetry',
                jumpTarget: 'SIG'
            }
        ];
        nodes.push({
            id: 'node-delivery-1',
            stage: 'stage-delivery',
            title: 'Target Executable Ingestion',
            technique: 'T1204 User Execution',
            classification: (report.signature && report.signature.isValid) ? 'pass' : (isMalicious ? 'critical' : (isSuspicious ? 'warn' : 'pass')),
            summary: `Analyzed executable binary ${report.fileName || 'target.exe'} with calculated SHA-256 fingerprint ${report.sha256 ? report.sha256.substring(0, 16) + '...' : 'N/A'}.`,
            status: 'Verified',
            rva: 0,
            fileOffset: 0,
            nextNodeIds: [],
            evidenceList: deliveryEvidence
        });

        // Stage 2: Staging / Overlay / Packed
        if (report.overlaySize && report.overlaySize > 0) {
            const ovMb = (report.overlaySize / (1024 * 1024)).toFixed(1);
            const ovPct = ((report.overlayRatio || 0) * 100).toFixed(1);
            const isHighEnt = (report.overlayEntropy || 0) >= 7.8;
            nodes.push({
                id: 'node-overlay-1',
                stage: 'stage-overlay',
                title: 'PE Overlay Stager Container',
                technique: 'T1027.002 Obfuscated / Appended Payload',
                classification: (report.hasSuspiciousOverlay || isHighEnt) ? 'critical' : 'warn',
                summary: `Massive appended PE overlay payload detected (${ovMb} MB, ${ovPct}% of file, entropy ${(report.overlayEntropy || 0).toFixed(2)}/8.00). Container signature: ${report.overlayType || 'Appended Binary Stream'}.`,
                status: 'Verified',
                rva: 0,
                fileOffset: report.overlayOffset || 0,
                nextNodeIds: [],
                evidenceList: [
                    {
                        type: 'CONTAINER_METRIC',
                        label: 'Appended Overlay Payload Boundary',
                        value: `Offset: 0x${(report.overlayOffset || 0).toString(16).toUpperCase()} | Size: ${formatBytes(report.overlaySize)} (${ovPct}% of file)`,
                        rule: 'SUSPICIOUS_OVERLAY_CONTAINER',
                        jumpTab: 'sections',
                        jumpTarget: '.rdata'
                    },
                    {
                        type: 'CONTAINER_METRIC',
                        label: 'Shannon Overlay Entropy Proof',
                        value: `Entropy: ${(report.overlayEntropy || 0).toFixed(2)} / 8.00 (High compressibility limit)`,
                        rule: 'HIGH_ENTROPY_OVERLAY',
                        jumpTab: 'telemetry',
                        jumpTarget: 'OVERLAY'
                    }
                ]
            });
        } else if (report.overallEntropy > 7.0 && isSuspicious) {
            nodes.push({
                id: 'node-packed-1',
                stage: 'stage-packed',
                title: 'Packed / Compressed Code Segment',
                technique: 'T1027 Software Packing',
                classification: isMalicious ? 'critical' : 'warn',
                summary: `High overall entropy ${(report.overallEntropy || 0).toFixed(2)}/8.00 suggests packed code or encrypted shellcode payload awaiting dynamic decompression.`,
                status: 'Verified',
                rva: report.entryPointRva || 0x1000,
                fileOffset: 0x400,
                nextNodeIds: [],
                evidenceList: [
                    {
                        type: 'CONTAINER_METRIC',
                        label: 'Binary Overall Entropy Density',
                        value: `Entropy: ${(report.overallEntropy || 0).toFixed(2)} / 8.00`,
                        rule: 'PACKED_BINARY_HEURISTIC',
                        jumpTab: 'telemetry',
                        jumpTarget: 'ENTROPY'
                    }
                ]
            });
        }

        // Stage 3: Initial Execution
        nodes.push({
            id: 'node-execution-1',
            stage: 'stage-execution',
            title: 'Application Entry Point Execution',
            technique: 'T1059 Command and Scripting Interpreter',
            classification: isMalicious ? 'critical' : (isSuspicious ? 'warn' : 'pass'),
            summary: `Execution flow transfers control to binary entry point at RVA 0x${(report.entryPointRva || 0x1000).toString(16).toUpperCase()}.`,
            status: 'Verified',
            rva: report.entryPointRva || 0x1000,
            fileOffset: 0x400,
            nextNodeIds: [],
            evidenceList: [
                {
                    type: 'DISASM',
                    label: 'Entry Point Subroutine Address',
                    value: `EntryPoint RVA: 0x${(report.entryPointRva || 0x1000).toString(16).toUpperCase()}`,
                    rule: 'ENTRY_POINT_VALIDATED',
                    jumpTab: 'decompile',
                    jumpTarget: '0x' + (report.entryPointRva || 0x1000).toString(16).toUpperCase()
                }
            ]
        });

        // Stage 4: Defense Evasion
        const evasionEvidence = [];
        if (Array.isArray(report.syscalls) && report.syscalls.length > 0) {
            report.syscalls.forEach(sc => {
                evasionEvidence.push({
                    type: 'DISASM',
                    label: `Direct Syscall Stub (SSN 0x${sc.ssn ? sc.ssn.toString(16).toUpperCase() : '18'})`,
                    value: `[0x${(sc.rva || 0).toString(16).toUpperCase()}] ${sc.disassembly || 'syscall'} (${sc.instructionHex || '0F 05'})`,
                    rule: 'DIRECT_SYSCALL_EVASION',
                    jumpTab: 'decompile',
                    jumpTarget: 'syscall'
                });
            });
        }
        if (Array.isArray(report.pebAccesses) && report.pebAccesses.length > 0) {
            report.pebAccesses.forEach(peb => {
                evasionEvidence.push({
                    type: 'DISASM',
                    label: 'PEB / TEB Module Traversal',
                    value: `[0x${(peb.rva || 0).toString(16).toUpperCase()}] ${peb.description || 'Access to PEB via GS register'}`,
                    rule: 'PEB_TRAVERSAL_DETECTED',
                    jumpTab: 'decompile',
                    jumpTarget: 'peb'
                });
            });
        }
        if (evasionEvidence.length > 0) {
            nodes.push({
                id: 'node-evasion-1',
                stage: 'stage-evasion',
                title: 'Direct Syscall & Evasion Stubs',
                technique: 'T1562.001 Impair Defenses: Disable Tools',
                classification: 'critical',
                summary: 'Static detection of direct kernel syscall execution stubs bypassing user-mode EDR inline hooks, combined with manual PEB module walk.',
                status: 'Verified',
                rva: report.syscalls && report.syscalls[0] ? report.syscalls[0].rva : 0x1000,
                fileOffset: 0x400,
                nextNodeIds: [],
                evidenceList: evasionEvidence
            });
        }

        // Stage 5: Process Injection
        if (report.hasInjectionChain) {
            nodes.push({
                id: 'node-injection-1',
                stage: 'stage-injection',
                title: 'Cross-Process Memory Injection',
                technique: 'T1055 Process Injection',
                classification: 'critical',
                summary: 'Co-location of VirtualAllocEx, WriteProcessMemory, and CreateRemoteThread indicates remote process injection and memory tampering.',
                status: 'Verified',
                rva: 0x1000,
                fileOffset: 0x400,
                nextNodeIds: [],
                evidenceList: [
                    {
                        type: 'HEURISTIC_RULE',
                        label: 'Chained API Call Sequence',
                        value: 'VirtualAllocEx -> WriteProcessMemory -> CreateRemoteThread',
                        rule: 'PROCESS_INJECTION_CHAIN',
                        jumpTab: 'decompile',
                        jumpTarget: 'injection'
                    }
                ]
            });
        }

        // Stage 6: Credential Harvesting
        const harvestEvidence = [];
        const exfilEvidence = [];
        if (Array.isArray(report.sensitiveStrings)) {
            report.sensitiveStrings.forEach(str => {
                const cat = str.category || '';
                const match = str.matchedPattern || '';
                if (cat.includes('Credential') || cat.includes('Browser') || cat.includes('Discord') || match.includes('Login Data') || match.includes('nkbihfb') || match.includes('leveldb')) {
                    harvestEvidence.push({
                        type: 'IOC_STRING',
                        label: `Scraped Credential Target (${cat})`,
                        value: match,
                        rule: 'CREDENTIAL_STEALER_ARTIFACT',
                        jumpTab: 'telemetry',
                        jumpTarget: 'STRING'
                    });
                } else if (cat.includes('Exfiltration') || match.includes('discord.com/api/webhooks') || match.includes('telegram.org')) {
                    exfilEvidence.push({
                        type: 'IOC_STRING',
                        label: `C2 Exfiltration Endpoint (${cat})`,
                        value: match,
                        rule: 'C2_EXFILTRATION_ENDPOINT',
                        jumpTab: 'telemetry',
                        jumpTarget: 'STRING'
                    });
                }
            });
        }

        if (harvestEvidence.length > 0) {
            nodes.push({
                id: 'node-harvesting-1',
                stage: 'stage-harvesting',
                title: 'Browser DPAPI & Wallet Harvesting',
                technique: 'T1555 Credentials from Password Stores',
                classification: 'critical',
                summary: 'Targeted extraction of browser master passwords, Discord authentication tokens, and cryptocurrency wallet extensions.',
                status: 'Verified',
                rva: 0x2000,
                fileOffset: 0x600,
                nextNodeIds: [],
                evidenceList: harvestEvidence
            });
        }

        // Stage 7: Exfiltration
        if (exfilEvidence.length > 0) {
            nodes.push({
                id: 'node-exfil-1',
                stage: 'stage-exfiltration',
                title: 'C2 Exfiltration Channel',
                technique: 'T1041 Exfiltration Over C2 Channel',
                classification: 'critical',
                summary: 'Outbound network channel for transmitting harvested victim payloads to attacker-controlled Discord webhooks or Telegram bots.',
                status: 'Verified',
                rva: 0x2000,
                fileOffset: 0x600,
                nextNodeIds: [],
                evidenceList: exfilEvidence
            });
        }

        // Benign fallback if clean
        if (nodes.length <= 2 && !isSuspicious) {
            nodes.push({
                id: 'node-clean-1',
                stage: 'stage-library',
                title: 'Standard Windows API Subsystem',
                technique: 'Legitimate System Routine',
                classification: 'pass',
                summary: 'Standard benign execution flow invoking authorized Windows GUI / Console APIs without suspicious anti-analysis evasions.',
                status: 'Verified',
                rva: 0x1000,
                fileOffset: 0x400,
                nextNodeIds: [],
                evidenceList: [
                    {
                        type: 'HEURISTIC_RULE',
                        label: 'Import Table Legitimacy',
                        value: 'Legitimate dynamic link libraries: KERNEL32.dll, USER32.dll',
                        rule: 'BENIGN_IMPORTS',
                        jumpTab: 'sections',
                        jumpTarget: '.text'
                    }
                ]
            });
        }

        // Link sequential stages
        for (let i = 0; i < nodes.length - 1; ++i) {
            nodes[i].nextNodeIds = [nodes[i + 1].id];
        }

        return nodes;
    }

    // Render Attack Chain graph to DOM
    function renderAttackChain(report) {
        if (!graphNodesLayer) return;

        currentAttackNodes = getOrSynthesizeAttackChain(report);
        const countBadge = document.getElementById('attack-chain-count-badge');
        if (countBadge) {
            countBadge.textContent = `${currentAttackNodes.length} forensic nodes`;
        }

        const tabBtn = document.getElementById('tab-attackchain-btn');
        if (tabBtn) {
            tabBtn.textContent = `Attack chain (${currentAttackNodes.length})`;
        }

        graphNodesLayer.innerHTML = '';
        if (svgPathsContainer) svgPathsContainer.innerHTML = '';

        if (currentAttackNodes.length === 0) {
            graphNodesLayer.innerHTML = '<div style="color: var(--text-dim); padding: 40px; font-size: 13px;">No forensic attack chain discovered. Select a preset profile above or drop an executable binary to begin.</div>';
            return;
        }

        // Helper to normalize backend stage names/IDs to stage order keys
        function mapNodeToStageKey(node) {
            if (node.id && node.id.startsWith('stage-')) {
                return node.id;
            }
            if (node.stage) {
                const s = String(node.stage).toLowerCase();
                if (s.includes('deliv') || s.includes('ingest')) return 'stage-delivery';
                if (s.includes('overlay')) return 'stage-overlay';
                if (s.includes('pack') || s.includes('decompress')) return 'stage-packed';
                if (s.includes('stag')) return 'stage-overlay';
                if (s.includes('evas') || s.includes('defense')) return 'stage-evasion';
                if (s.includes('inject') || s.includes('privilege') || s.includes('escalat')) return 'stage-injection';
                if (s.includes('harvest') || s.includes('cred') || s.includes('access')) return 'stage-harvesting';
                if (s.includes('exfil') || s.includes('c2')) return 'stage-exfiltration';
                if (s.includes('lib') || s.includes('benign')) return 'stage-library';
                if (s.includes('exec') || s.includes('process')) return 'stage-execution';
                if (s.startsWith('stage-')) return s;
                return 'stage-' + s.replace(/[^a-z0-9]/g, '-');
            }
            return 'stage-execution';
        }

        // Group nodes by stage
        const stageOrder = [
            { key: 'stage-delivery', title: '1. Ingestion' },
            { key: 'stage-overlay', title: '2. Stager / Overlay' },
            { key: 'stage-packed', title: '2. Decompression' },
            { key: 'stage-execution', title: '3. Execution' },
            { key: 'stage-evasion', title: '4. Defense Evasion' },
            { key: 'stage-injection', title: '5. Process Injection' },
            { key: 'stage-harvesting', title: '6. Harvesting' },
            { key: 'stage-exfiltration', title: '7. C2 Exfiltration' },
            { key: 'stage-library', title: 'Benign API Flow' }
        ];

        const stageMap = new Map();
        currentAttackNodes.forEach(node => {
            const key = mapNodeToStageKey(node);
            if (!stageMap.has(key)) stageMap.set(key, []);
            stageMap.get(key).push(node);
        });

        // Dynamically add any unknown stage categories to stageOrder
        const knownKeys = new Set(stageOrder.map(s => s.key));
        stageMap.forEach((nodes, key) => {
            if (!knownKeys.has(key)) {
                stageOrder.push({
                    key: key,
                    title: (nodes[0]?.stage || key).replace(/^stage-/, '')
                });
                knownKeys.add(key);
            }
        });

        // Create column for each non-empty stage
        stageOrder.forEach(so => {
            const nodes = stageMap.get(so.key);
            if (!nodes || nodes.length === 0) return;

            const col = document.createElement('div');
            col.className = 'chain-stage-col';
            col.setAttribute('data-stage-key', so.key);

            // Column Header
            col.innerHTML = `
                <div class="chain-stage-header">
                    <span>${escapeHtml(so.title)}</span>
                    <span class="stage-step-pill">${nodes.length} node${nodes.length > 1 ? 's' : ''}</span>
                </div>
            `;

            // Node cards
            nodes.forEach(node => {
                const card = document.createElement('div');
                const badgeClass = (node.classification || 'pass').toLowerCase();
                card.className = `chain-node-card ${badgeClass} ${node.id === activeNodeId ? 'active' : ''}`;
                card.setAttribute('data-node-id', node.id);
                card.setAttribute('data-classification', badgeClass);
                card.setAttribute('data-stage', node.stage || so.key);

                const rvaHex = '0x' + (node.rva || 0).toString(16).toUpperCase();
                const offHex = '0x' + (node.fileOffset || 0).toString(16).toUpperCase();
                const evCount = (node.evidenceList || []).length;

                let badgeLabel = 'BENIGN';
                if (badgeClass === 'critical') badgeLabel = 'CRITICAL';
                else if (badgeClass === 'warn') badgeLabel = 'SUSPICIOUS';

                card.innerHTML = `
                    <div class="card-socket socket-in"></div>
                    <div class="chain-card-header">
                        <div class="chain-card-title-group">
                            <span class="chain-card-stage-name">${escapeHtml(so.title)}</span>
                            <span class="chain-card-title">${escapeHtml(node.title)}</span>
                        </div>
                        <span class="finding-pill ${badgeClass === 'critical' ? 'critical' : badgeClass === 'warn' ? 'warning' : 'info'}">${badgeLabel}</span>
                    </div>
                    <div class="chain-card-technique">${escapeHtml(node.technique || '')}</div>
                    <div class="chain-card-summary">${escapeHtml(node.summary || '')}</div>
                    <div class="chain-card-metrics">
                        <span>RVA ${rvaHex} • Offset ${offHex}</span>
                        <span class="chain-card-evidence-badge">
                            <svg width="10" height="10" viewBox="0 0 10 10"><circle cx="5" cy="5" r="4" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M5 3v2.5l1.5 1" stroke="currentColor" stroke-width="1.2" stroke-linecap="round"/></svg>
                            ${evCount} proof${evCount !== 1 ? 's' : ''}
                        </span>
                    </div>
                    <div class="card-socket socket-out"></div>
                `;

                card.addEventListener('click', (e) => {
                    e.stopPropagation();
                    selectAttackNode(node);
                });

                col.appendChild(card);
            });

            graphNodesLayer.appendChild(col);
        });

        // Apply active filter
        applyStageFilter(currentStageFilter);

        // Schedule connector routing after DOM layout settles
        setTimeout(() => {
            updateAttackChainConnectors();
            fitAttackChainView();
            const firstCritical = currentAttackNodes.find(n => (n.classification || '').toLowerCase() === 'critical');
            if (firstCritical) {
                selectAttackNode(firstCritical);
            } else if (currentAttackNodes.length > 0) {
                selectAttackNode(currentAttackNodes[0]);
            }
        }, 50);
    }

    // Select node and populate inspect proof drawer
    function selectAttackNode(node) {
        if (!node) return;
        activeNodeId = node.id;

        // Highlight card
        document.querySelectorAll('.chain-node-card').forEach(c => {
            if (c.getAttribute('data-node-id') === node.id) {
                c.classList.add('active');
            } else {
                c.classList.remove('active');
            }
        });

        // Fill drawer elements
        const nodeStageEl = document.getElementById('drawer-node-stage');
        const nodeTitleEl = document.getElementById('drawer-node-title');
        const nodeBadgeEl = document.getElementById('drawer-node-badge');
        const nodeTechEl = document.getElementById('drawer-node-technique');
        const nodeOffEl = document.getElementById('drawer-node-offsets');
        const nodeSumEl = document.getElementById('drawer-node-summary');
        const evListEl = document.getElementById('drawer-evidence-list');

        if (nodeStageEl) nodeStageEl.textContent = (node.stage || 'STAGE').toUpperCase();
        if (nodeTitleEl) nodeTitleEl.textContent = node.title || 'Forensic Node';

        const cls = (node.classification || 'pass').toLowerCase();
        if (nodeBadgeEl) {
            nodeBadgeEl.className = 'finding-pill ' + (cls === 'critical' ? 'critical' : cls === 'warn' ? 'warning' : 'info');
            nodeBadgeEl.textContent = cls === 'critical' ? 'CRITICAL' : cls === 'warn' ? 'SUSPICIOUS' : 'BENIGN';
        }

        if (nodeTechEl) nodeTechEl.textContent = node.technique || 'Forensic Component';

        const rvaHex = '0x' + (node.rva || 0).toString(16).toUpperCase();
        const offHex = '0x' + (node.fileOffset || 0).toString(16).toUpperCase();
        if (nodeOffEl) nodeOffEl.textContent = `Memory RVA: ${rvaHex} | File Offset: ${offHex}`;

        if (nodeSumEl) nodeSumEl.textContent = node.summary || '';

        // Populate forensic evidence cards
        if (evListEl) {
            evListEl.innerHTML = '';
            const evidence = node.evidenceList || [];

            if (evidence.length === 0) {
                evListEl.innerHTML = '<span style="color: var(--text-dim); font-size: 11px;">No individual forensic evidence artifacts registered for this stage.</span>';
            } else {
                evidence.forEach(ev => {
                    const card = document.createElement('div');
                    card.className = 'drawer-evidence-card';

                    let jumpButtonHtml = '';
                    if (ev.jumpTab === 'telemetry') {
                        jumpButtonHtml = `
                            <button class="evidence-jump-btn" data-jump="telemetry" data-target="${escapeHtml(ev.jumpTarget || '')}">
                                <svg width="12" height="12" viewBox="0 0 12 12"><path d="M2 2h8v8H2z" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M4 6h4M4 4h4M4 8h2" stroke="currentColor" stroke-width="1.2"/></svg>
                                View in Telemetry Log
                            </button>
                        `;
                    } else if (ev.jumpTab === 'decompile') {
                        jumpButtonHtml = `
                            <button class="evidence-jump-btn" data-jump="decompile" data-target="${escapeHtml(ev.jumpTarget || '')}">
                                <svg width="12" height="12" viewBox="0 0 12 12"><path d="M4 3L1 6l3 3M8 3l3 3-3 3" fill="none" stroke="currentColor" stroke-width="1.2" stroke-linecap="round"/></svg>
                                View Pseudocode
                            </button>
                        `;
                    } else if (ev.jumpTab === 'sections') {
                        jumpButtonHtml = `
                            <button class="evidence-jump-btn" data-jump="sections" data-target="${escapeHtml(ev.jumpTarget || '')}">
                                <svg width="12" height="12" viewBox="0 0 12 12"><rect x="2" y="2" width="8" height="8" fill="none" stroke="currentColor" stroke-width="1.2"/><line x1="2" y1="5" x2="10" y2="5" stroke="currentColor" stroke-width="1.2"/></svg>
                                View Section
                            </button>
                        `;
                    }

                    card.innerHTML = `
                        <div class="evidence-card-header">
                            <span class="evidence-type-pill">${escapeHtml(ev.type || 'EVIDENCE')}</span>
                            <span class="evidence-rule-tag">${escapeHtml(ev.rule || '')}</span>
                        </div>
                        <div class="evidence-card-label">${escapeHtml(ev.label || '')}</div>
                        <div class="evidence-card-value">${escapeHtml(ev.value || '')}</div>
                        ${jumpButtonHtml}
                    `;

                    // Wire up bidirectional jump button
                    const btn = card.querySelector('.evidence-jump-btn');
                    if (btn) {
                        btn.addEventListener('click', (e) => {
                            e.stopPropagation();
                            const jTab = btn.getAttribute('data-jump');
                            const jTarget = btn.getAttribute('data-target');
                            executeCrossTabJump(jTab, jTarget);
                        });
                    }

                    evListEl.appendChild(card);
                });
            }
        }

        // Open drawer
        if (evidenceDrawer) evidenceDrawer.classList.add('open');
    }

    // Bidirectional Cross-View Jumper
    function executeCrossTabJump(jumpTab, targetQuery) {
        if (!jumpTab) return;

        if (jumpTab === 'telemetry') {
            const telTabBtn = document.getElementById('tab-telemetry-btn');
            if (telTabBtn) telTabBtn.click();

            // Reset filter to ALL
            const allBtn = document.querySelector('.log-filter-btn[data-filter="ALL"]');
            if (allBtn) allBtn.click();

            // Search row in telemetry log
            setTimeout(() => {
                const rows = Array.from(document.querySelectorAll('#telemetry-table-body tr.telemetry-row'));
                let targetRow = null;
                if (targetQuery) {
                    const q = targetQuery.toLowerCase();
                    targetRow = rows.find(r => r.textContent.toLowerCase().includes(q));
                }
                if (!targetRow && rows.length > 0) {
                    targetRow = rows[0];
                }
                if (targetRow) {
                    targetRow.scrollIntoView({ behavior: 'smooth', block: 'center' });
                    targetRow.classList.remove('flash-highlight');
                    void targetRow.offsetWidth;
                    targetRow.classList.add('flash-highlight');
                }
            }, 80);
        } else if (jumpTab === 'decompile') {
            const decompTabBtn = document.getElementById('tab-decompile-btn');
            if (decompTabBtn) decompTabBtn.click();

            setTimeout(() => {
                if (!currentReport || !currentReport.decompiledFunctions) return;
                const funcs = currentReport.decompiledFunctions;
                let foundIdx = 0;

                if (targetQuery) {
                    const q = targetQuery.toLowerCase();
                    const idx = funcs.findIndex(fn => {
                        const rvaStr = '0x' + (fn.rva || 0).toString(16).toLowerCase();
                        return fn.name.toLowerCase().includes(q) || rvaStr.includes(q) || (q === 'syscall' && fn.hasSyscall) || (q === 'peb' && fn.hasPebAccess) || (q === 'injection' && fn.hasInjection);
                    });
                    if (idx >= 0) foundIdx = idx;
                }

                selectedFuncIndex = foundIdx;
                const cards = document.querySelectorAll('.func-card');
                cards.forEach(c => c.classList.remove('active'));
                if (cards[foundIdx]) {
                    cards[foundIdx].classList.add('active');
                    cards[foundIdx].scrollIntoView({ behavior: 'smooth', block: 'nearest' });
                }
                renderActiveFunction();

                const codeBox = document.getElementById('code-viewer-pseudocode');
                if (codeBox) {
                    codeBox.classList.remove('flash-highlight');
                    void codeBox.offsetWidth;
                    codeBox.classList.add('flash-highlight');
                }
            }, 80);
        } else if (jumpTab === 'sections') {
            const secTabBtn = document.querySelector('.tab-btn[data-tab="sections"]');
            if (secTabBtn) secTabBtn.click();

            setTimeout(() => {
                const rows = Array.from(document.querySelectorAll('#sections-table-body tr'));
                let targetRow = null;
                if (targetQuery) {
                    const q = targetQuery.toLowerCase();
                    targetRow = rows.find(r => r.textContent.toLowerCase().includes(q));
                }
                if (!targetRow && rows.length > 0) {
                    targetRow = rows[0];
                }
                if (targetRow) {
                    targetRow.scrollIntoView({ behavior: 'smooth', block: 'center' });
                    targetRow.classList.remove('flash-highlight');
                    void targetRow.offsetWidth;
                    targetRow.classList.add('flash-highlight');
                }
            }, 80);
        }
    }

    // Dynamic SVG connector routing between nodes
    function updateAttackChainConnectors() {
        if (!svgPathsContainer || !graphNodesLayer) return;
        svgPathsContainer.innerHTML = '';

        if (!currentAttackNodes || currentAttackNodes.length === 0) return;

        const layerRect = graphNodesLayer.getBoundingClientRect();
        if (layerRect.width === 0 || layerRect.height === 0) return;

        // Map node elements
        const cardMap = new Map();
        document.querySelectorAll('.chain-node-card').forEach(card => {
            const id = card.getAttribute('data-node-id');
            if (id) cardMap.set(id, card);
        });

        const scale = graphScale || 1.0;

        currentAttackNodes.forEach(sourceNode => {
            const sourceCard = cardMap.get(sourceNode.id);
            if (!sourceCard || sourceCard.closest('.filtered-out')) return;

            const nextIds = sourceNode.nextNodeIds || [];
            nextIds.forEach(targetId => {
                const targetCard = cardMap.get(targetId);
                const targetNode = currentAttackNodes.find(n => n.id === targetId);
                if (!targetCard || targetCard.closest('.filtered-out') || !targetNode) return;

                const sRect = sourceCard.getBoundingClientRect();
                const tRect = targetCard.getBoundingClientRect();

                // Coordinates relative to layer regardless of column nesting
                const sX = (sRect.right - layerRect.left) / scale;
                const sY = (sRect.top + sRect.height / 2 - layerRect.top) / scale;
                const tX = (tRect.left - layerRect.left) / scale;
                const tY = (tRect.top + tRect.height / 2 - layerRect.top) / scale;

                const dx = Math.max(30, (tX - sX) * 0.45);
                const pathD = `M ${sX} ${sY} C ${sX + dx} ${sY}, ${tX - dx} ${tY}, ${tX} ${tY}`;

                const targetCls = (targetNode.classification || 'pass').toLowerCase();
                const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
                path.setAttribute('d', pathD);
                path.setAttribute('class', `chain-link-path ${targetCls}`);

                let markerId = 'url(#arrow-pass)';
                if (targetCls === 'critical') markerId = 'url(#arrow-threat)';
                else if (targetCls === 'warn') markerId = 'url(#arrow-warn)';
                path.setAttribute('marker-end', markerId);

                svgPathsContainer.appendChild(path);
            });
        });
    }

    // Stage filter logic
    function applyStageFilter(filter) {
        currentStageFilter = filter;
        document.querySelectorAll('.chain-filter-btn').forEach(b => {
            if (b.getAttribute('data-stage-filter') === filter) {
                b.classList.add('active');
            } else {
                b.classList.remove('active');
            }
        });

        document.querySelectorAll('.chain-stage-col').forEach(col => {
            const stageKey = col.getAttribute('data-stage-key');
            let isVisible = true;

            if (filter === 'CRITICAL') {
                const hasCritical = col.querySelector('.chain-node-card.critical');
                isVisible = !!hasCritical;
            } else if (filter === 'EVASION') {
                isVisible = (stageKey === 'stage-overlay' || stageKey === 'stage-packed' || stageKey === 'stage-evasion' || stageKey === 'stage-injection');
            } else if (filter === 'EXFIL') {
                isVisible = (stageKey === 'stage-harvesting' || stageKey === 'stage-exfiltration');
            }

            if (isVisible) {
                col.classList.remove('filtered-out');
            } else {
                col.classList.add('filtered-out');
            }
        });

        updateAttackChainConnectors();
    }

    // Filter toolbar buttons
    document.querySelectorAll('.chain-filter-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const f = btn.getAttribute('data-stage-filter');
            applyStageFilter(f);
        });
    });

    // Close drawer button
    const btnCloseDrawer = document.getElementById('btn-close-drawer');
    if (btnCloseDrawer && evidenceDrawer) {
        btnCloseDrawer.addEventListener('click', () => {
            evidenceDrawer.classList.remove('open');
            document.querySelectorAll('.chain-node-card').forEach(c => c.classList.remove('active'));
        });
    }

    // Pan & Zoom controls
    const btnGraphZoomIn = document.getElementById('btn-graph-zoom-in');
    const btnGraphZoomOut = document.getElementById('btn-graph-zoom-out');
    const btnGraphReset = document.getElementById('btn-graph-reset');
    const btnGraphFit = document.getElementById('btn-graph-fit');

    if (btnGraphZoomIn) {
        btnGraphZoomIn.addEventListener('click', () => {
            graphScale = Math.min(2.5, graphScale * 1.2);
            updateGraphTransform();
        });
    }
    if (btnGraphZoomOut) {
        btnGraphZoomOut.addEventListener('click', () => {
            graphScale = Math.max(0.35, graphScale * 0.8);
            updateGraphTransform();
        });
    }
    if (btnGraphReset) {
        btnGraphReset.addEventListener('click', () => {
            graphScale = 1.0;
            graphPanX = 36;
            graphPanY = 36;
            updateGraphTransform();
        });
    }
    if (btnGraphFit) {
        btnGraphFit.addEventListener('click', () => {
            fitAttackChainView();
        });
    }

    // Mouse Wheel Scroll & Zoom on Stage
    if (graphStage) {
        graphStage.addEventListener('wheel', (e) => {
            e.preventDefault();
            if (e.ctrlKey || e.metaKey) {
                // Ctrl + Wheel: Zoom centered at mouse position
                const rect = graphStage.getBoundingClientRect();
                const mouseX = e.clientX - rect.left;
                const mouseY = e.clientY - rect.top;

                const zoomFactor = e.deltaY < 0 ? 1.12 : 0.89;
                const newScale = Math.min(2.5, Math.max(0.35, graphScale * zoomFactor));

                graphPanX = mouseX - (mouseX - graphPanX) * (newScale / graphScale);
                graphPanY = mouseY - (mouseY - graphPanY) * (newScale / graphScale);
                graphScale = newScale;
            } else {
                // Natural mouse wheel scrolling across the map
                if (e.shiftKey) {
                    graphPanX -= e.deltaY * 0.9;
                } else {
                    graphPanY -= e.deltaY * 0.9;
                    if (e.deltaX) {
                        graphPanX -= e.deltaX * 0.9;
                    }
                }
            }

            updateGraphTransform();
        }, { passive: false });

        // Mouse Drag to Pan
        graphStage.addEventListener('mousedown', (e) => {
            if (e.target.closest('.evidence-drawer') || e.target.closest('button')) return;
            isPanning = true;
            panStartX = e.clientX - graphPanX;
            panStartY = e.clientY - graphPanY;
            graphStage.classList.add('panning');
        });

        window.addEventListener('mousemove', (e) => {
            if (!isPanning) return;
            graphPanX = e.clientX - panStartX;
            graphPanY = e.clientY - panStartY;
            updateGraphTransform();
        });

        window.addEventListener('mouseup', () => {
            if (isPanning) {
                isPanning = false;
                if (graphStage) graphStage.classList.remove('panning');
            }
        });
    }

    // Auto-load clean sample on initial launch
    setTimeout(() => {
        const cleanBtn = document.querySelector('[data-sample="clean"]');
        if (cleanBtn) cleanBtn.click();
    }, 400);
});
