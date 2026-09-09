import type { Asset, Segment } from '../domain'

function grams(text: string): Set<string> {
  const normalized = text.toLowerCase().replace(/\s+/g, '')
  const result = new Set<string>()
  for (let index = 0; index < normalized.length; index += 1) {
    result.add(normalized[index])
    if (index + 1 < normalized.length) result.add(normalized.slice(index, index + 2))
  }
  return result
}

export function retrieveSegments(assets: Asset[], brief: string, limit = 80): Segment[] {
  const query = grams(brief)
  return assets.flatMap((asset) => asset.segments).map((segment) => {
    const tokens = grams(segment.text)
    let overlap = 0
    for (const token of query) if (tokens.has(token)) overlap += token.length
    const density = overlap / Math.max(1, query.size)
    return { segment, score: density + Math.min(segment.text.length, 60) / 600 }
  }).sort((a, b) => b.score - a.score).slice(0, limit).map((item) => item.segment)
}

export function makeFallbackPlan(assets: Asset[], brief: string) {
  const picked = retrieveSegments(assets, brief, 12).filter((segment) => segment.end > segment.start)
  let cursor = 0
  return {
    version: '1.0' as const,
    title: 'ShotClip 规则粗剪',
    summary: '本地模型尚未配置，按文字关键词生成的可审查演示方案。',
    targetDuration: picked.reduce((sum, item) => sum + item.end - item.start, 0),
    clips: picked.map((segment, index) => {
      const clip = {
        id: `fallback-${index + 1}`,
        assetId: segment.assetId,
        sourceIn: segment.start,
        sourceOut: segment.end,
        timelineStart: cursor,
        videoTrack: 1,
        audioTrack: 1,
        reason: '与用户要求存在关键词关联，需人工确认上下文。',
        evidenceSegmentIds: [segment.id],
      }
      cursor += segment.end - segment.start
      return clip
    }),
    warnings: ['这是关键词备用模式，不代表语义理解结果。'],
  }
}
