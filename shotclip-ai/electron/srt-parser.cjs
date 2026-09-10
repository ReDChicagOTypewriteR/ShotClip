function parseTimestamp(value) {
  const match = /^(\d+):(\d{2}):(\d{2})[,.](\d{1,3})$/.exec(value.trim())
  if (!match) return null
  const milliseconds = Number(match[4].padEnd(3, '0'))
  const seconds = Number(match[1]) * 3600 + Number(match[2]) * 60 + Number(match[3]) + milliseconds / 1000
  return Number.isFinite(seconds) ? seconds : null
}
function parseWhisperSrt(text, assetId, offset) {
  const normalized = text.replace(/\r/g, '').replace(/^\uFEFF/, '').trim()
  if (!normalized) return { segments: [], skipped: 0 }
  const segments = []
  let skipped = 0
  for (const [index, block] of normalized.split(/\n\s*\n/).entries()) {
    const lines = block.split('\n')
    const timeIndex = lines.findIndex((line) => line.includes('-->'))
    if (timeIndex < 0) { skipped += 1; continue }
    const [startValue, endValue] = lines[timeIndex].split('-->').map((value) => value.trim())
    const localStart = startValue ? parseTimestamp(startValue) : null
    const localEnd = endValue ? parseTimestamp(endValue) : null
    const content = lines.slice(timeIndex + 1).join(' ').replace(/\s+/g, ' ').trim()
    const isSilenceMarker = /^\[(?:blank_audio|silence|no speech)\]$/i.test(content)
    if (localStart === null || localEnd === null || localEnd <= localStart || !content || isSilenceMarker) { skipped += 1; continue }
    const start = offset + localStart
    const end = offset + localEnd
    segments.push({ id: `${assetId}:${Math.round(start * 1000)}:${index}`, assetId, start, end, text: content })
  }
  return { segments, skipped }
}
module.exports = { parseWhisperSrt }
