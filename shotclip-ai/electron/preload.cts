const { contextBridge, ipcRenderer } = require('electron')

contextBridge.exposeInMainWorld('shotclip', {
  selectMedia: () => ipcRenderer.invoke('dialog:media'),
  selectMediaFolder: () => ipcRenderer.invoke('dialog:media-folder'),
  selectFile: (kind: string) => ipcRenderer.invoke('dialog:file', kind),
  selectDirectory: () => ipcRenderer.invoke('dialog:directory'),
  loadSettings: () => ipcRenderer.invoke('settings:load'),
  saveSettings: (settings: unknown) => ipcRenderer.invoke('settings:save', settings),
  probe: (paths: string[]) => ipcRenderer.invoke('media:probe', paths),
  transcribe: (asset: unknown, settings: unknown) => ipcRenderer.invoke('media:transcribe', asset, settings),
  generatePlan: (payload: unknown) => ipcRenderer.invoke('ai:plan', payload),
  rankSegments: (payload: unknown) => ipcRenderer.invoke('ai:rank', payload),
  saveProject: (project: unknown, saveAs = false) => ipcRenderer.invoke('project:save', project, saveAs),
  openProject: () => ipcRenderer.invoke('project:open'),
  checkpointProject: (project: unknown) => ipcRenderer.invoke('project:checkpoint', project),
  recoverProject: () => ipcRenderer.invoke('project:recover'),
  exportPackage: (payload: unknown) => ipcRenderer.invoke('export:package', payload),
  cancelTask: () => ipcRenderer.invoke('task:cancel'),
  onEvent: (callback: (event: unknown) => void) => {
    const listener = (_event: unknown, value: unknown) => callback(value)
    ipcRenderer.on('workflow:event', listener)
    return () => ipcRenderer.removeListener('workflow:event', listener)
  },
})
