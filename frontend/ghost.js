// Koltzi Living Ghost Companion - Procedural Canvas Animation Engine
// Features: Floating physics, pupil tracking, blinking, blushing, mood auras, particles

class GhostCompanion {
    constructor(canvas, bubbleElement) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.bubble = bubbleElement;

        this.mood = 'idle'; // 'idle', 'sniffing', 'happy', 'alarmed', 'puzzled'
        this.theme = 'ember'; // 'clean', 'ember', 'malware', 'midnight', 'twilight', 'black', 'light'
        this.currentDialogue = 'Ready for binary analysis. Drop a PE binary or select a sample preset to begin.';
        this.displayedDialogue = '';
        this.typewriterIndex = 0;
        this.typewriterTimer = 0;

        // Settings flags
        this.enabled = true;
        this.particlesEnabled = true;
        this.trackingEnabled = true;
        this.reducedMotion = false;
        this.onMoodChange = null;

        // Positioning & physics
        this.x = 0;
        this.y = 0;
        this.baseY = 0;
        this.time = 0;
        this.floatOffset = 0;
        this.tilt = 0;

        // Eye tracking & blinking
        this.targetMouseX = 0;
        this.targetMouseY = 0;
        this.lookX = 0;
        this.lookY = 0;
        this.blinkState = 0; // 0 = open, 1 = fully closed
        this.blinkTimer = 3.0;
        this.isBlinking = false;

        // Particles
        this.particles = [];
        this.initParticles();

        this.resize();
        window.addEventListener('resize', () => this.resize());

        // Mouse tracking
        window.addEventListener('mousemove', (e) => {
            if (!this.trackingEnabled || this.reducedMotion) {
                this.targetMouseX = 0;
                this.targetMouseY = 0;
                return;
            }
            const rect = this.canvas.getBoundingClientRect();
            this.targetMouseX = (e.clientX - (rect.left + rect.width / 2)) / (rect.width / 2);
            this.targetMouseY = (e.clientY - (rect.top + rect.height / 2)) / (rect.height / 2);
            this.targetMouseX = Math.max(-1, Math.min(1, this.targetMouseX));
            this.targetMouseY = Math.max(-1, Math.min(1, this.targetMouseY));
        });

        // Click to interact
        this.canvas.addEventListener('click', () => {
            this.triggerPoke();
        });

        this.setDialogue(this.currentDialogue, true);
    }

    initParticles() {
        this.particles = [];
        for (let i = 0; i < 22; i++) {
            this.particles.push({
                x: (Math.random() - 0.5) * 140,
                y: (Math.random() - 0.5) * 120,
                vx: (Math.random() - 0.5) * 0.35,
                vy: -Math.random() * 0.45 - 0.15,
                size: Math.random() * 2.2 + 1.0,
                alpha: Math.random() * 0.6 + 0.2,
                life: Math.random() * 100,
                maxLife: 100 + Math.random() * 70
            });
        }
    }

    resize() {
        const dpr = window.devicePixelRatio || 1;
        const rect = this.canvas.getBoundingClientRect();
        this.canvas.width = rect.width * dpr;
        this.canvas.height = rect.height * dpr;
        this.ctx.scale(dpr, dpr);
        this.width = rect.width;
        this.height = rect.height;
        this.x = this.width / 2;
        this.baseY = this.height / 2 - 8;
    }

    setMood(mood) {
        const oldMood = this.mood;
        this.mood = mood;
        if (oldMood !== mood && typeof this.onMoodChange === 'function') {
            this.onMoodChange(mood);
        }
    }

    setTheme(theme) {
        this.theme = theme;
    }

    setDialogue(text, immediate = false) {
        this.currentDialogue = text;
        if (immediate) {
            this.displayedDialogue = text;
            this.typewriterIndex = text.length;
            if (this.bubble) this.bubble.textContent = text;
        } else {
            this.typewriterIndex = 0;
            this.displayedDialogue = '';
            if (this.bubble) this.bubble.textContent = '';
        }
    }

    triggerPoke() {
        const quips = {
            idle: [
                'Standing by. Ready to parse headers and disasm bytes.',
                'Feed me an EXE, DLL, or SYS binary to inspect!',
                'All sensors online. Zero network telemetry leaked.'
            ],
            sniffing: [
                'Scanning instruction stream for unhooked syscalls...',
                'Calculating Shannon entropy density across PE sections...'
            ],
            happy: [
                'Clean verdict! Imports and headers look completely benign.',
                'Valid certificate and standard entropy distribution.'
            ],
            alarmed: [
                'High alert! Critical indicators detected in this sample!',
                'Direct syscall stubs or injection primitives identified.'
            ],
            puzzled: [
                'Curious packing detected. Section entropy exceeds 7.20!',
                'Heuristic discrepancies found. Analysis recommended.'
            ]
        };

        const list = quips[this.mood] || quips.idle;
        const picked = list[Math.floor(Math.random() * list.length)];
        this.setDialogue(picked, false);

        // Fun jiggle
        if (!this.reducedMotion) {
            this.floatOffset -= 8;
            this.tilt = (Math.random() - 0.5) * 0.16;
        }
    }

    update(dt) {
        this.time += dt;

        if (this.reducedMotion) {
            this.floatOffset = 0;
            this.tilt = 0;
            this.y = this.baseY;
        } else {
            // Floating bobbing physics
            const bobSpeed = (this.mood === 'alarmed') ? 3.8 : (this.mood === 'sniffing') ? 3.2 : 2.0;
            const bobAmp = (this.mood === 'happy') ? 8 : 6;
            this.floatOffset = Math.sin(this.time * bobSpeed) * bobAmp;
            this.tilt += (-Math.sin(this.time * (bobSpeed * 0.7)) * 0.035 - this.tilt) * Math.min(1, dt * 6);
            this.y = this.baseY + this.floatOffset;
        }

        // Smooth pupil tracking
        this.lookX += (this.targetMouseX - this.lookX) * Math.min(1, dt * 8);
        this.lookY += (this.targetMouseY - this.lookY) * Math.min(1, dt * 8);

        // Blinking logic
        this.blinkTimer -= dt;
        if (this.blinkTimer <= 0) {
            if (!this.isBlinking) {
                this.isBlinking = true;
                this.blinkTimer = 0.14; // Blink duration
            } else {
                this.isBlinking = false;
                this.blinkState = 0;
                this.blinkTimer = 2.5 + Math.random() * 4.0;
            }
        }

        if (this.isBlinking) {
            this.blinkState = Math.min(1, this.blinkState + dt * 14);
        } else {
            this.blinkState = Math.max(0, this.blinkState - dt * 14);
        }

        // Particle physics
        for (const p of this.particles) {
            p.x += p.vx;
            p.y += p.vy;
            p.life += dt * 30;
            if (p.life >= p.maxLife || p.y < -75) {
                p.x = (Math.random() - 0.5) * 120;
                p.y = 50 + Math.random() * 25;
                p.life = 0;
                p.alpha = Math.random() * 0.6 + 0.2;
            }
        }

        // Typewriter update
        if (this.typewriterIndex < this.currentDialogue.length) {
            this.typewriterTimer += dt;
            if (this.typewriterTimer > 0.02) {
                this.typewriterTimer = 0;
                this.typewriterIndex++;
                this.displayedDialogue = this.currentDialogue.substring(0, this.typewriterIndex);
                if (this.bubble) this.bubble.textContent = this.displayedDialogue;
            }
        }
    }

    render() {
        const ctx = this.ctx;
        ctx.clearRect(0, 0, this.width, this.height);
        if (!this.enabled) return;

        ctx.save();
        ctx.translate(this.x, this.y);
        ctx.rotate(this.tilt);

        // 1. Mood Aura
        this.renderAura(ctx);

        // 2. Ambient Particles
        if (this.particlesEnabled && !this.reducedMotion) {
            this.renderParticles(ctx);
        }

        // 3. Ghost Body
        this.renderBody(ctx);

        // 4. Ghost Arms
        this.renderArms(ctx);

        // 5. Ghost Face (Eyes, Cheeks, Mouth)
        this.renderFace(ctx);

        // 6. Mood Accents (Question mark, sparkles, alerts)
        this.renderMoodAccessories(ctx);

        ctx.restore();
    }

    renderAura(ctx) {
        let auraColor = 'rgba(245, 158, 11, 0.24)'; // Default Ember
        if (this.theme === 'clean') auraColor = 'rgba(16, 185, 129, 0.30)';
        else if (this.theme === 'malware') auraColor = 'rgba(239, 68, 68, 0.32)';
        else if (this.theme === 'midnight') auraColor = 'rgba(56, 189, 248, 0.26)';
        else if (this.theme === 'twilight') auraColor = 'rgba(167, 139, 250, 0.26)';
        else if (this.theme === 'black') auraColor = 'rgba(255, 255, 255, 0.14)';
        else if (this.theme === 'light') auraColor = 'rgba(37, 99, 235, 0.24)';

        if (this.mood === 'happy') auraColor = 'rgba(16, 185, 129, 0.32)'; // Clean emerald
        else if (this.mood === 'alarmed') auraColor = 'rgba(239, 68, 68, 0.35)'; // Crimson malware
        else if (this.mood === 'puzzled') {
            auraColor = (this.theme === 'clean') ? 'rgba(52, 211, 153, 0.28)' :
                        (this.theme === 'malware') ? 'rgba(248, 113, 113, 0.30)' :
                        (this.theme === 'twilight') ? 'rgba(192, 132, 252, 0.30)' : 'rgba(245, 158, 11, 0.28)';
        } else if (this.mood === 'sniffing') {
            auraColor = (this.theme === 'clean') ? 'rgba(16, 185, 129, 0.30)' :
                        (this.theme === 'malware') ? 'rgba(239, 68, 68, 0.32)' :
                        (this.theme === 'midnight') ? 'rgba(56, 189, 248, 0.28)' :
                        (this.theme === 'twilight') ? 'rgba(192, 132, 252, 0.28)' : 'rgba(245, 158, 11, 0.26)';
        }

        const rad = 70 + Math.sin(this.time * 3.0) * 4;
        const grad = ctx.createRadialGradient(0, -6, 12, 0, -6, rad);
        grad.addColorStop(0, auraColor);
        grad.addColorStop(0.65, auraColor.replace(/[\d\.]+\)$/, '0.08)'));
        grad.addColorStop(1, 'rgba(0,0,0,0)');

        ctx.fillStyle = grad;
        ctx.beginPath();
        ctx.arc(0, -6, rad, 0, Math.PI * 2);
        ctx.fill();
    }

    renderParticles(ctx) {
        let pColor = '251, 191, 36';
        if (this.theme === 'clean') pColor = '52, 211, 153';
        else if (this.theme === 'malware') pColor = '248, 113, 113';
        else if (this.theme === 'midnight') pColor = '56, 189, 248';
        else if (this.theme === 'twilight') pColor = '192, 132, 252';
        else if (this.theme === 'black') pColor = '255, 255, 255';
        else if (this.theme === 'light') pColor = '37, 99, 235';

        if (this.mood === 'happy') pColor = '52, 211, 153';
        else if (this.mood === 'alarmed') pColor = '248, 113, 113';
        else if (this.mood === 'puzzled') {
            pColor = (this.theme === 'clean') ? '52, 211, 153' :
                     (this.theme === 'malware') ? '248, 113, 113' : '251, 191, 36';
        } else if (this.mood === 'sniffing') {
            pColor = (this.theme === 'clean') ? '52, 211, 153' :
                     (this.theme === 'malware') ? '248, 113, 113' :
                     (this.theme === 'midnight') ? '56, 189, 248' :
                     (this.theme === 'twilight') ? '192, 132, 252' : '251, 191, 36';
        }

        for (const p of this.particles) {
            const lifeRatio = 1 - (p.life / p.maxLife);
            const a = p.alpha * lifeRatio;
            ctx.fillStyle = `rgba(${pColor}, ${a})`;
            ctx.beginPath();
            ctx.arc(p.x, p.y, p.size, 0, Math.PI * 2);
            ctx.fill();
        }
    }

    renderBody(ctx) {
        ctx.save();

        // Ghost outer gradient: luminous frosted white to soft cyan-slate
        const bodyGrad = ctx.createLinearGradient(0, -85, 0, 75);
        bodyGrad.addColorStop(0, 'rgba(255, 255, 255, 0.98)');
        bodyGrad.addColorStop(0.55, 'rgba(240, 245, 255, 0.92)');
        bodyGrad.addColorStop(1, 'rgba(215, 225, 240, 0.85)');

        ctx.fillStyle = bodyGrad;
        ctx.shadowColor = 'rgba(255, 255, 255, 0.35)';
        ctx.shadowBlur = 18;

        const w = 56;
        const h = 75;

        // Smooth dome head and wavy skirt
        ctx.beginPath();
        ctx.moveTo(-w, 15);
        // Head dome
        ctx.bezierCurveTo(-w, -h, w, -h, w, 15);
        // Right side down
        ctx.lineTo(w, 55);

        // Dynamic flowing bottom skirt waves
        const waveT = this.time * 4.5;
        const skirtY1 = 65 + Math.sin(waveT) * 4;
        const skirtY2 = 60 + Math.sin(waveT + 1.2) * 5;
        const skirtY3 = 66 + Math.sin(waveT + 2.4) * 4;
        const skirtY4 = 62 + Math.sin(waveT + 3.6) * 4;

        ctx.bezierCurveTo(w - 12, skirtY1, w - 24, skirtY2, w - 38, 55);
        ctx.bezierCurveTo(w - 50, skirtY3, w - 62, skirtY4, w - 75, 55);
        ctx.bezierCurveTo(w - 88, skirtY1, w - 100, skirtY2, -w, 55);

        ctx.closePath();
        ctx.fill();

        // Inner specular highlight rim hugging the inner dome curve
        ctx.shadowBlur = 0;
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.40)';
        ctx.lineWidth = 1.4;
        ctx.beginPath();
        ctx.moveTo(-36, 0);
        ctx.bezierCurveTo(-36, -42, 36, -42, 36, 0);
        ctx.stroke();

        ctx.restore();
    }

    renderArms(ctx) {
        ctx.save();
        ctx.fillStyle = 'rgba(240, 245, 255, 0.95)';

        // Left arm
        const lArmWave = Math.sin(this.time * 2.8) * 4;
        ctx.beginPath();
        ctx.ellipse(-52, 10 + lArmWave, 9, 14, -0.3, 0, Math.PI * 2);
        ctx.fill();

        // Right arm (waves when happy/sniffing)
        const rArmWave = (this.mood === 'happy') ? Math.sin(this.time * 6.5) * 8 : Math.cos(this.time * 2.5) * 4;
        const rAngle = (this.mood === 'happy') ? 0.6 : 0.3;
        ctx.beginPath();
        ctx.ellipse(52, 10 + rArmWave, 9, 14, rAngle, 0, Math.PI * 2);
        ctx.fill();

        ctx.restore();
    }

    renderFace(ctx) {
        const eyeOffsetX = this.lookX * 5;
        const eyeOffsetY = this.lookY * 4;

        // Eye positions
        const leftEyeX = -20 + eyeOffsetX;
        const rightEyeX = 20 + eyeOffsetX;
        const eyeY = -12 + eyeOffsetY;

        // 1. Cheeks (Blushing)
        const blushAlpha = (this.mood === 'happy') ? 0.65 : 0.35;
        ctx.fillStyle = `rgba(244, 114, 182, ${blushAlpha})`;
        ctx.beginPath();
        ctx.ellipse(-32 + eyeOffsetX * 0.4, 6 + eyeOffsetY * 0.3, 8, 4.5, 0, 0, Math.PI * 2);
        ctx.ellipse(32 + eyeOffsetX * 0.4, 6 + eyeOffsetY * 0.3, 8, 4.5, 0, 0, Math.PI * 2);
        ctx.fill();

        // 2. Eyes
        if (this.mood === 'happy') {
            // Smiling crescent eyes ^ ^
            ctx.strokeStyle = '#0f172a';
            ctx.lineWidth = 3.5;
            ctx.lineCap = 'round';

            ctx.beginPath();
            ctx.arc(leftEyeX, eyeY + 2, 8, Math.PI * 1.1, Math.PI * 1.9);
            ctx.stroke();

            ctx.beginPath();
            ctx.arc(rightEyeX, eyeY + 2, 8, Math.PI * 1.1, Math.PI * 1.9);
            ctx.stroke();

            // Tiny happy mouth
            ctx.beginPath();
            ctx.arc(0 + eyeOffsetX * 0.5, eyeY + 14, 5, 0, Math.PI);
            ctx.fillStyle = '#0f172a';
            ctx.fill();

        } else {
            // Expressive round eyes with pupils and highlights
            const eyeHeight = 13 * (1 - this.blinkState);

            if (eyeHeight > 0.8) {
                // Eye sockets
                ctx.fillStyle = '#0f172a';
                ctx.beginPath();
                ctx.ellipse(leftEyeX, eyeY, 8.5, eyeHeight, 0, 0, Math.PI * 2);
                ctx.ellipse(rightEyeX, eyeY, 8.5, eyeHeight, 0, 0, Math.PI * 2);
                ctx.fill();

                // Pupil / Highlight
                const pupilRad = (this.mood === 'alarmed') ? 2.2 : 3.5;
                ctx.fillStyle = '#ffffff';
                ctx.beginPath();
                ctx.arc(leftEyeX - 2.5, eyeY - 2.5, pupilRad, 0, Math.PI * 2);
                ctx.arc(rightEyeX - 2.5, eyeY - 2.5, pupilRad, 0, Math.PI * 2);
                ctx.fill();

                // Secondary shine
                ctx.fillStyle = 'rgba(255, 255, 255, 0.7)';
                ctx.beginPath();
                ctx.arc(leftEyeX + 3.0, eyeY + 2.5, 1.6, 0, Math.PI * 2);
                ctx.arc(rightEyeX + 3.0, eyeY + 2.5, 1.6, 0, Math.PI * 2);
                ctx.fill();
            } else {
                // Closed eyelid slit
                ctx.strokeStyle = '#0f172a';
                ctx.lineWidth = 2.5;
                ctx.beginPath();
                ctx.moveTo(leftEyeX - 7, eyeY);
                ctx.lineTo(leftEyeX + 7, eyeY);
                ctx.moveTo(rightEyeX - 7, eyeY);
                ctx.lineTo(rightEyeX + 7, eyeY);
                ctx.stroke();
            }

            // Mouth
            if (this.mood === 'alarmed') {
                // Shocked small O mouth
                ctx.fillStyle = '#0f172a';
                ctx.beginPath();
                ctx.ellipse(0 + eyeOffsetX * 0.5, eyeY + 15, 4, 6, 0, 0, Math.PI * 2);
                ctx.fill();
            } else if (this.mood === 'puzzled') {
                // Slight wavy line mouth
                ctx.strokeStyle = '#0f172a';
                ctx.lineWidth = 2.5;
                ctx.lineCap = 'round';
                ctx.beginPath();
                ctx.moveTo(-5 + eyeOffsetX * 0.5, eyeY + 14);
                ctx.lineTo(5 + eyeOffsetX * 0.5, eyeY + 15);
                ctx.stroke();
            } else {
                // Cute neutral smile
                ctx.strokeStyle = '#0f172a';
                ctx.lineWidth = 2.2;
                ctx.lineCap = 'round';
                ctx.beginPath();
                ctx.arc(0 + eyeOffsetX * 0.5, eyeY + 12, 4.5, 0.1 * Math.PI, 0.9 * Math.PI);
                ctx.stroke();
            }
        }
    }

    renderMoodAccessories(ctx) {
        if (this.mood === 'puzzled') {
            // Floating amber question mark near crest of head
            ctx.save();
            ctx.font = 'bold 20px "JetBrains Mono", monospace';
            ctx.fillStyle = '#f59e0b';
            ctx.shadowColor = 'rgba(245, 158, 11, 0.6)';
            ctx.shadowBlur = 8;
            const qY = -48 + Math.sin(this.time * 4) * 3;
            ctx.fillText('?', 32, qY);
            ctx.restore();
        } else if (this.mood === 'alarmed') {
            // Soft crimson exclamation badge
            ctx.save();
            ctx.font = 'bold 20px "JetBrains Mono", monospace';
            ctx.fillStyle = '#ef4444';
            ctx.shadowColor = 'rgba(239, 68, 68, 0.8)';
            ctx.shadowBlur = 10;
            const aY = -48 + Math.sin(this.time * 6) * 3;
            ctx.fillText('!', 34, aY);
            ctx.restore();
        } else if (this.mood === 'sniffing') {
            // Small radar scanning wave ring
            ctx.save();
            const rRadius = 10 + (this.time * 24) % 24;
            const rAlpha = 1 - ((this.time * 24) % 24) / 24;
            ctx.strokeStyle = (this.theme === 'midnight') ? `rgba(56, 189, 248, ${rAlpha * 0.7})` :
                              (this.theme === 'twilight') ? `rgba(167, 139, 250, ${rAlpha * 0.7})` :
                              `rgba(245, 158, 11, ${rAlpha * 0.7})`;
            ctx.lineWidth = 1.2;
            ctx.beginPath();
            ctx.arc(0, -52, rRadius, 0, Math.PI * 2);
            ctx.stroke();
            ctx.restore();
        } else if (this.mood === 'happy') {
            // Sparkle stars
            ctx.save();
            ctx.fillStyle = '#34d399';
            ctx.shadowColor = 'rgba(52, 211, 153, 0.8)';
            ctx.shadowBlur = 8;
            const starY = -50 + Math.sin(this.time * 3) * 3.5;
            this.drawStar(ctx, 36, starY, 4, 5.5, 2.2);
            ctx.restore();
        }
    }

    drawStar(ctx, cx, cy, spikes, outerRadius, innerRadius) {
        let rot = Math.PI / 2 * 3;
        let x = cx;
        let y = cy;
        const step = Math.PI / spikes;

        ctx.beginPath();
        ctx.moveTo(cx, cy - outerRadius);
        for (let i = 0; i < spikes; i++) {
            x = cx + Math.cos(rot) * outerRadius;
            y = cy + Math.sin(rot) * outerRadius;
            ctx.lineTo(x, y);
            rot += step;

            x = cx + Math.cos(rot) * innerRadius;
            y = cy + Math.sin(rot) * innerRadius;
            ctx.lineTo(x, y);
            rot += step;
        }
        ctx.lineTo(cx, cy - outerRadius);
        ctx.closePath();
        ctx.fill();
    }
}

window.GhostCompanion = GhostCompanion;
