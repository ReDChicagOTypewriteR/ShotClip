import { create } from 'zustand'
import type { Asset, EditPlan, ModelSettings, Project } from './domain'
import { emptySettings } from './domain'

type AppStore = {
  project: Project & { filePath?: string }
  settings: ModelSettings
  selectedAssetId?: string
  busy: boolean
  taskProgress?: number
  logs: WorkflowEvent[]
  setProject(project: Project & { filePath?: string }): void
  setSettings(settings: ModelSettings): void
  setSelectedAsset(id?: string): void
  setBusy(busy: boolean): void
  setTaskProgress(progress?: number): void
  addAssets(assets: Asset[]): void
  replaceAsset(asset: Asset): void
  setBrief(brief: string): void
  setPlan(plan?: EditPlan): void
  addLog(event: WorkflowEvent): void
}

const now = new Date().toISOString()
const initialProject: Project = {
  version: 1, id: crypto.randomUUID(), name: '未命名工程', createdAt: now, updatedAt: now,
  fps: 25, width: 1920, height: 1080, assets: [], brief: '',
}

export const useAppStore = create<AppStore>((set) => ({
  project: initialProject,
  settings: emptySettings,
  busy: false,
  logs: [],
  setProject: (project) => set({ project }),
  setSettings: (settings) => set({ settings }),
  setSelectedAsset: (selectedAssetId) => set({ selectedAssetId }),
  setBusy: (busy) => set({ busy, ...(busy ? {} : { taskProgress: undefined }) }),
  setTaskProgress: (taskProgress) => set({ taskProgress }),
  addAssets: (assets) => set((state) => ({ project: { ...state.project, assets: [...state.project.assets, ...assets], updatedAt: new Date().toISOString() } })),
  replaceAsset: (asset) => set((state) => ({ project: { ...state.project, assets: state.project.assets.map((item) => item.id === asset.id ? asset : item), updatedAt: new Date().toISOString() } })),
  setBrief: (brief) => set((state) => ({ project: { ...state.project, brief } })),
  setPlan: (plan) => set((state) => ({ project: { ...state.project, plan, updatedAt: new Date().toISOString() } })),
  addLog: (event) => set((state) => ({ logs: [...state.logs.slice(-499), event] })),
}))
