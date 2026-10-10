const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const { execFile } = require('child_process');
const fs = require('fs');

// Prevent disk cache access conflicts and redundant network/shader caches on Windows
app.commandLine.appendSwitch('disable-gpu-shader-disk-cache');
app.commandLine.appendSwitch('disable-http-cache');

// Enforce single instance lock to prevent concurrent process cache collisions
const gotSingleInstanceLock = app.requestSingleInstanceLock();
if (!gotSingleInstanceLock) {
    app.quit();
    process.exit(0);
}

app.on('second-instance', () => {
    if (mainWindow) {
        if (mainWindow.isMinimized()) mainWindow.restore();
        mainWindow.focus();
    }
});

let mainWindow = null;

const KOLTZI_EXE = path.join(__dirname, '..', 'build_rel', 'Koltzi.exe');

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1360,
        height: 880,
        minWidth: 1080,
        minHeight: 700,
        frame: false,
        backgroundColor: '#100d0a',
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            nodeIntegration: false,
            contextIsolation: true
        },
        show: false
    });

    mainWindow.loadFile(path.join(__dirname, 'index.html'));

    // Prevent window navigation if a file is dropped
    mainWindow.webContents.on('will-navigate', (event) => {
        event.preventDefault();
    });

    mainWindow.once('ready-to-show', () => {
        mainWindow.show();
    });

    mainWindow.on('closed', () => {
        mainWindow = null;
    });
}

app.whenReady().then(() => {
    createWindow();

    app.on('activate', () => {
        if (BrowserWindow.getAllWindows().length === 0) createWindow();
    });
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') app.quit();
});

// Window controls
ipcMain.handle('window-minimize', () => {
    if (mainWindow) mainWindow.minimize();
});

ipcMain.handle('window-maximize', () => {
    if (mainWindow) {
        if (mainWindow.isMaximized()) mainWindow.unmaximize();
        else mainWindow.maximize();
    }
});

ipcMain.handle('window-close', () => {
    if (mainWindow) mainWindow.close();
});

// File picker
ipcMain.handle('select-file', async () => {
    if (!mainWindow) return null;
    const result = await dialog.showOpenDialog(mainWindow, {
        title: 'Select PE Binary to Analyze',
        properties: ['openFile'],
        filters: [
            { name: 'PE Executables', extensions: ['exe', 'dll', 'sys'] },
            { name: 'All Files', extensions: ['*'] }
        ]
    });
    if (result.canceled || result.filePaths.length === 0) return null;
    return result.filePaths[0];
});

// Run native C++ Koltzi engine on a file
ipcMain.handle('analyze-file', async (event, filePath) => {
    return new Promise((resolve) => {
        if (!fs.existsSync(KOLTZI_EXE)) {
            resolve({
                parseSuccess: false,
                parseError: 'Koltzi.exe engine not found in build_rel directory. Run run.ps1 -Build first.'
            });
            return;
        }

        execFile(KOLTZI_EXE, ['--json', filePath], { maxBuffer: 10 * 1024 * 1024 }, (error, stdout, stderr) => {
            if (error && !stdout) {
                resolve({
                    parseSuccess: false,
                    parseError: error.message || stderr || 'Execution failed'
                });
                return;
            }

            try {
                const report = JSON.parse(stdout.trim());
                resolve(report);
            } catch (parseErr) {
                resolve({
                    parseSuccess: false,
                    parseError: 'Failed to parse engine JSON output: ' + parseErr.message
                });
            }
        });
    });
});

// Run native C++ Koltzi engine on a built-in sample
ipcMain.handle('analyze-sample', async (event, sampleType) => {
    return new Promise((resolve) => {
        if (!fs.existsSync(KOLTZI_EXE)) {
            resolve({
                parseSuccess: false,
                parseError: 'Koltzi.exe engine not found in build_rel directory.'
            });
            return;
        }

        execFile(KOLTZI_EXE, ['--json-sample', sampleType], { maxBuffer: 10 * 1024 * 1024 }, (error, stdout, stderr) => {
            if (error && !stdout) {
                resolve({
                    parseSuccess: false,
                    parseError: error.message || stderr || 'Sample execution failed'
                });
                return;
            }

            try {
                const report = JSON.parse(stdout.trim());
                resolve(report);
            } catch (parseErr) {
                resolve({
                    parseSuccess: false,
                    parseError: 'Failed to parse sample JSON output: ' + parseErr.message
                });
            }
        });
    });
});
