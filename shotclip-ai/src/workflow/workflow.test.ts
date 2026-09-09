import { describe, expect, it } from 'vitest'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { assetSchema, validatePlan, type Project } from '../domain'
import { compilePremiereXml, compileSrt } from './premiereXml'
import { makeFallbackPlan, retrieveSegments } from './retrieval'

const asset = assetSchema.parse({ id: 'a1', name: '中文 素材.mp4', path: 'C:\\素材 库\\中文 素材.mp4', kind: 'video', duration: 60, width: 1920, height: 1080, fps: 25, hasAudio: true, status: 'ready', progress: 1, fingerprint: 'x', segments: [
  { id: 's1', assetId: 'a1', start: 2, end: 8, text: '我最后悔的是让家里人承受了这些' },
  { id: 's2', assetId: 'a1', start: 12, end: 17, text: '希望年轻人不要走错路' },
] })
const project: Project = { version: 1, id: 'p1', name: '测试', createdAt: '', updatedAt: '', fps: 25, width: 1920, height: 1080, assets: [asset], brief: '家庭影响' }

describe('offline editorial workflow', () => {
  it('retrieves Chinese evidence and creates a reviewable fallback', () => {
    expect(retrieveSegments([asset], '家庭影响')[0].id).toBe('s1')
    const plan = validatePlan(makeFallbackPlan([asset], '家庭影响'), [asset])
    expect(plan.clips[0].evidenceSegmentIds).toEqual(['s1'])
  })
  it('rejects invented evidence and overlapping timeline clips', () => {
    const plan = makeFallbackPlan([asset], '家庭影响')
    plan.clips[0].evidenceSegmentIds = ['invented']
    expect(() => validatePlan(plan, [asset])).toThrow(/无效证据/)
    const overlap = makeFallbackPlan([asset], '家庭影响')
    overlap.clips[1].timelineStart = 0
    expect(() => validatePlan(overlap, [asset])).toThrow(/重叠/)
  })
  it('compiles FCP7 XML and SRT without exposing malformed paths', () => {
    const plan = validatePlan(makeFallbackPlan([asset], '家庭影响'), [asset])
    const xml = compilePremiereXml(project, plan)
    expect(xml).toContain('<xmeml version="5">')
    expect(xml).toContain('file://localhost/C:/%E7%B4%A0%E6%9D%90%20%E5%BA%93/')
    expect(xml).toContain('<timebase>25</timebase>')
    expect(compileSrt(project, plan)).toContain('我最后悔')
  })
  it.skipIf(spawnSync('ffmpeg', ['-version']).status !== 0)('probes an automatically generated Chinese-path media fixture', () => {
    const directory = mkdtempSync(join(tmpdir(), 'shotclip-中文-'))
    const media = join(directory, '演讲 样本.mp4')
    try {
      execFileSync('ffmpeg', ['-v', 'error', '-f', 'lavfi', '-i', 'color=c=black:s=640x360:r=25:d=2', '-f', 'lavfi', '-i', 'sine=frequency=440:duration=2', '-shortest', '-c:v', 'libx264', '-c:a', 'aac', '-y', media])
      const output = execFileSync('ffprobe', ['-v', 'error', '-show_format', '-show_streams', '-of', 'json', media], { encoding: 'utf8' })
      const metadata = JSON.parse(output)
      expect(Number(metadata.format.duration)).toBeGreaterThan(1.9)
      expect(metadata.streams.some((stream: { codec_type: string }) => stream.codec_type === 'video')).toBe(true)
      expect(metadata.streams.some((stream: { codec_type: string }) => stream.codec_type === 'audio')).toBe(true)
    } finally { rmSync(directory, { recursive: true, force: true }) }
  })
})
