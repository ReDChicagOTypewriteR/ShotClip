import type { Asset, ModelSettings, Project } from './domain'

declare global {
  interface Window {
    shotclip: {
      selectMedia(): Promise<string[]>
      selectMediaFolder(): Promise<string[]>
      selectFile(kind: 'bin' | 'gguf' | 'exe'): Promise<string>
      selectDirectory(): Promise<string>
      loadSettings(): Promise<Partial<ModelSettings>>
      saveSettings(settings: ModelSettings): Promise<boolean>
      probe(paths: string[]): Promise<Asset[]>
      transcribe(asset: Asset, settings: ModelSettings): Promise<Asset>
      generatePlan(payload: unknown): Promise<unknown>
      rankSegments(payload: unknown): Promise<string[] | null>
      saveProject(project: Project & { filePath?: string }, saveAs?: boolean): Promise<string | null>
      openProject(): Promise<(Project & { filePath?: string }) | null>
      checkpointProject(project: Project & { filePath?: string }): Promise<boolean>
      recoverProject(): Promise<(Project & { filePath?: string }) | null>
      exportPackage(payload: unknown): Promise<string | null>
      cancelTask(): Promise<boolean>
      onEvent(callback: (event: WorkflowEvent) => void): () => void
    }
  }
  type WorkflowEvent = { type: 'status' | 'log' | 'error'; message: string; progress?: number; at: string }
}

export {}
