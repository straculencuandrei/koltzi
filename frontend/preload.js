const { contextBridge, ipcRenderer, webUtils } = require('electron');

contextBridge.exposeInMainWorld('koltzi', {
    minimize: () => ipcRenderer.invoke('window-minimize'),
    maximize: () => ipcRenderer.invoke('window-maximize'),
    close: () => ipcRenderer.invoke('window-close'),
    selectFile: () => ipcRenderer.invoke('select-file'),
    analyzeFile: (filePath) => ipcRenderer.invoke('analyze-file', filePath),
    analyzeSample: (sampleType) => ipcRenderer.invoke('analyze-sample', sampleType),
    getPathForFile: (file) => {
        try {
            if (webUtils && typeof webUtils.getPathForFile === 'function') {
                return webUtils.getPathForFile(file);
            }
        } catch (e) {
            return file.path || '';
        }
        return file.path || '';
    }
});

// Global drag and drop capture in preload context
window.addEventListener('dragover', (event) => {
    event.preventDefault();
    if (event.dataTransfer) {
        event.dataTransfer.dropEffect = 'copy';
    }
}, true);

window.addEventListener('drop', (event) => {
    event.preventDefault();
    try {
        const dt = event.dataTransfer;
        if (dt && dt.files && dt.files.length > 0) {
            const file = dt.files[0];
            let resolvedPath = '';
            try {
                if (webUtils && typeof webUtils.getPathForFile === 'function') {
                    resolvedPath = webUtils.getPathForFile(file);
                }
            } catch (err) {
                console.warn('webUtils.getPathForFile in preload drop failed:', err);
            }
            if (!resolvedPath && file.path) {
                resolvedPath = file.path;
            }
            if (resolvedPath) {
                window.dispatchEvent(new CustomEvent('koltzi-file-dropped', { detail: { filePath: resolvedPath } }));
            }
        }
    } catch (err) {
        console.error('Preload drop handler error:', err);
    }
}, true);
