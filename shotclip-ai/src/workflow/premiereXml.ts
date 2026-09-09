import type { Asset, EditPlan, Project } from '../domain'

const escapeXml = (value: string) => value.replace(/[&<>"']/g, (character) => ({
  '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&apos;',
}[character] ?? character))

const frames = (seconds: number, fps: number) => Math.round(seconds * fps)

function fileUrl(filePath: string): string {
  const normalized = filePath.replace(/\\/g, '/')
  const withSlash = normalized.startsWith('/') ? normalized : `/${normalized}`
  return `file://localhost${encodeURI(withSlash).replace(/#/g, '%23')}`
}

export function compilePremiereXml(project: Project, plan: EditPlan): string {
  const fps = project.fps
  const assetMap = new Map<string, Asset>(project.assets.map((asset) => [asset.id, asset]))
  const sequenceDuration = Math.max(...plan.clips.map((clip) => clip.timelineStart + clip.sourceOut - clip.sourceIn))
  const files = new Set<string>()
  const video: string[] = []
  const audio: string[] = []
  plan.clips.forEach((clip, index) => {
    const asset = assetMap.get(clip.assetId)
    if (!asset) throw new Error(`Missing asset ${clip.assetId}`)
    const fileId = `file-${asset.id}`
    const mediaDuration = frames(asset.duration, fps)
    const fileBlock = files.has(fileId) ? `<file id="${escapeXml(fileId)}"/>` : `<file id="${escapeXml(fileId)}"><name>${escapeXml(asset.name)}</name><pathurl>${escapeXml(fileUrl(asset.path))}</pathurl><duration>${mediaDuration}</duration><rate><timebase>${fps}</timebase><ntsc>FALSE</ntsc></rate><media><video/><audio/></media></file>`
    files.add(fileId)
    const common = `<name>${escapeXml(asset.name)}</name><duration>${mediaDuration}</duration><rate><timebase>${fps}</timebase><ntsc>FALSE</ntsc></rate><start>${frames(clip.timelineStart, fps)}</start><end>${frames(clip.timelineStart + clip.sourceOut - clip.sourceIn, fps)}</end><in>${frames(clip.sourceIn, fps)}</in><out>${frames(clip.sourceOut, fps)}</out>${fileBlock}`
    video.push(`<clipitem id="video-${index + 1}">${common}<comments>${escapeXml(clip.reason)}</comments><link><linkclipref>audio-${index + 1}</linkclipref></link></clipitem>`)
    if (asset.hasAudio) audio.push(`<clipitem id="audio-${index + 1}">${common}<sourcetrack><mediatype>audio</mediatype><trackindex>1</trackindex></sourcetrack><link><linkclipref>video-${index + 1}</linkclipref></link></clipitem>`)
  })
  return `<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE xmeml>\n<xmeml version="5"><sequence id="sequence-1"><name>${escapeXml(plan.title)}</name><duration>${frames(sequenceDuration, fps)}</duration><rate><timebase>${fps}</timebase><ntsc>FALSE</ntsc></rate><media><video><format><samplecharacteristics><rate><timebase>${fps}</timebase><ntsc>FALSE</ntsc></rate><width>${project.width}</width><height>${project.height}</height><anamorphic>FALSE</anamorphic><pixelaspectratio>square</pixelaspectratio><fielddominance>none</fielddominance></samplecharacteristics></format><track>${video.join('')}</track></video><audio><format><samplecharacteristics><depth>16</depth><samplerate>48000</samplerate></samplecharacteristics></format><track>${audio.join('')}</track></audio></media></sequence></xmeml>\n`
}

export function compileSrt(project: Project, plan: EditPlan): string {
  const segmentMap = new Map(project.assets.flatMap((asset) => asset.segments.map((segment) => [segment.id, segment] as const)))
  const stamp = (seconds: number) => {
    const milliseconds = Math.max(0, Math.round(seconds * 1000))
    const hours = Math.floor(milliseconds / 3_600_000)
    const minutes = Math.floor(milliseconds / 60_000) % 60
    const secs = Math.floor(milliseconds / 1000) % 60
    const millis = milliseconds % 1000
    return [hours, minutes, secs].map((part) => String(part).padStart(2, '0')).join(':') + `,${String(millis).padStart(3, '0')}`
  }
  const rows: string[] = []
  for (const clip of plan.clips) for (const evidenceId of clip.evidenceSegmentIds) {
    const segment = segmentMap.get(evidenceId)
    if (!segment) continue
    const start = clip.timelineStart + Math.max(segment.start, clip.sourceIn) - clip.sourceIn
    const end = clip.timelineStart + Math.min(segment.end, clip.sourceOut) - clip.sourceIn
    if (end > start) rows.push(`${rows.length + 1}\n${stamp(start)} --> ${stamp(end)}\n${segment.text}`)
  }
  return `${rows.join('\n\n')}\n`
}
