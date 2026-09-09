import { z } from 'zod'

export const segmentSchema = z.object({
  id: z.string().min(1),
  assetId: z.string().min(1),
  start: z.number().finite().nonnegative(),
  end: z.number().finite().positive(),
  text: z.string().min(1),
  speaker: z.string().optional(),
  confidence: z.number().min(0).max(1).optional(),
}).refine((v) => v.end > v.start, 'segment end must be after start')

export const assetSchema = z.object({
  id: z.string().min(1),
  name: z.string().min(1),
  path: z.string().min(1),
  kind: z.enum(['video', 'audio', 'image']),
  duration: z.number().finite().nonnegative(),
  width: z.number().int().nonnegative().default(0),
  height: z.number().int().nonnegative().default(0),
  fps: z.number().finite().positive().default(25),
  hasAudio: z.boolean().default(false),
  status: z.enum(['pending', 'analyzing', 'ready', 'failed']).default('pending'),
  progress: z.number().min(0).max(1).default(0),
  error: z.string().optional(),
  segments: z.array(segmentSchema).default([]),
  fingerprint: z.string().min(1),
})

export const clipSchema = z.object({
  id: z.string().min(1),
  assetId: z.string().min(1),
  sourceIn: z.number().finite().nonnegative(),
  sourceOut: z.number().finite().positive(),
  timelineStart: z.number().finite().nonnegative(),
  videoTrack: z.number().int().min(1).max(16).default(1),
  audioTrack: z.number().int().min(1).max(16).default(1),
  reason: z.string().min(1),
  evidenceSegmentIds: z.array(z.string()).min(1),
}).refine((v) => v.sourceOut > v.sourceIn, 'clip out must be after in')

export const planSchema = z.object({
  version: z.literal('1.0'),
  title: z.string().min(1),
  summary: z.string().min(1),
  targetDuration: z.number().finite().positive(),
  clips: z.array(clipSchema).min(1),
  warnings: z.array(z.string()).default([]),
}).strict()

export type Segment = z.infer<typeof segmentSchema>
export type Asset = z.infer<typeof assetSchema>
export type EditPlan = z.infer<typeof planSchema>

export type Project = {
  version: 1
  id: string
  name: string
  createdAt: string
  updatedAt: string
  fps: number
  width: number
  height: number
  assets: Asset[]
  brief: string
  plan?: EditPlan
}

export const projectSchema = z.object({
  version: z.literal(1), id: z.string().min(1), name: z.string().min(1),
  createdAt: z.string(), updatedAt: z.string(), fps: z.number().finite().positive(),
  width: z.number().int().positive(), height: z.number().int().positive(),
  assets: z.array(assetSchema), brief: z.string(), plan: planSchema.optional(), filePath: z.string().optional(),
})

export type ModelSettings = {
  ffmpegPath: string
  ffprobePath: string
  whisperPath: string
  whisperModelPath: string
  llamaPath: string
  llamaModelPath: string
  embeddingPath: string
  embeddingModelPath: string
  gpuLayers: number
  contextSize: number
}

export const emptySettings: ModelSettings = {
  ffmpegPath: '', ffprobePath: '', whisperPath: '', whisperModelPath: '',
  llamaPath: '', llamaModelPath: '', embeddingPath: '', embeddingModelPath: '',
  gpuLayers: 999, contextSize: 8192,
}

export function validatePlan(raw: unknown, assets: Asset[]): EditPlan {
  const plan = planSchema.parse(raw)
  const assetMap = new Map(assets.map((asset) => [asset.id, asset]))
  const evidence = new Map(assets.flatMap((asset) => asset.segments.map((segment) => [segment.id, segment] as const)))
  const clipIds = new Set<string>()
  let previousEnd = 0
  for (const clip of plan.clips) {
    if (clipIds.has(clip.id)) throw new Error(`方案包含重复片段 ID：${clip.id}`)
    clipIds.add(clip.id)
    const asset = assetMap.get(clip.assetId)
    if (!asset) throw new Error(`方案引用了不存在的素材：${clip.assetId}`)
    if (clip.sourceOut > asset.duration + 0.001) throw new Error(`片段 ${clip.id} 超出素材时长`)
    if (clip.timelineStart + 0.001 < previousEnd) throw new Error(`片段 ${clip.id} 在主序列中发生重叠`)
    for (const id of clip.evidenceSegmentIds) {
      const segment = evidence.get(id)
      if (!segment || segment.assetId !== clip.assetId) throw new Error(`片段 ${clip.id} 引用了无效证据`)
      if (segment.end < clip.sourceIn || segment.start > clip.sourceOut) throw new Error(`片段 ${clip.id} 的证据不在选取范围内`)
    }
    previousEnd = clip.timelineStart + clip.sourceOut - clip.sourceIn
  }
  return plan
}
