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
            return webUtils.getPathForFile(file);
        } catch (e) {
            return file.path || '';
        }
    }
});
