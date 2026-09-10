const { app, BrowserWindow, dialog, ipcMain } = require('electron')
const { spawn } = require('node:child_process')
const crypto = require('node:crypto')
const fs = require('node:fs/promises')
const fsSync = require('node:fs')
const os = require('node:os')
const path = require('node:path')
const { parseWhisperSrt } = require('./srt-parser.cjs')

let mainWindow: typeof BrowserWindow | null = null
let activeProcess: import('node:child_process').ChildProcessWithoutNullStreams | null = null
let cancelled = false

const emit = (type: string, message: string, progress?: number) => {
  mainWindow?.webContents.send('workflow:event', { type, message, progress, at: new Date().toISOString() })
}

function safeError(error: unknown) {
  return error instanceof Error ? error.message : String(error)
}

async function atomicWrite(filePath: string, contents: string) {
  await fs.mkdir(path.dirname(filePath), { recursive: true })
  const temporary = `${filePath}.${process.pid}.tmp`
  await fs.writeFile(temporary, contents, 'utf8')
  await fs.rename(temporary, filePath)
}

async function loadModelSettings() {
  const platform = process.platform === 'win32' ? 'win32-x64' : process.platform === 'darwin' ? `darwin-${process.arch}` : `linux-${process.arch}`
  const runtime = path.join(process.resourcesPath, 'runtime', platform)
  const executable = (name: string, family: 'ffmpeg' | 'whisper' | 'llama') => {
    const fileName = process.platform === 'win32' ? `${name}.exe` : name
    const candidates = [path.join(runtime, family, fileName), path.join(runtime, fileName)]
    return candidates.find((candidate) => fsSync.existsSync(candidate)) || ''
  }
  const defaults = {
    ffmpegPath: executable('ffmpeg', 'ffmpeg'),
    ffprobePath: executable('ffprobe', 'ffmpeg'),
    whisperPath: executable('whisper-cli', 'whisper'),
    llamaPath: executable('llama-cli', 'llama'),
    embeddingPath: executable('llama-embedding', 'llama'),
  }
  try { return { ...defaults, ...JSON.parse(await fs.readFile(path.join(app.getPath('userData'), 'model-settings.json'), 'utf8')) } }
  catch { return defaults }
}

function validateModelSettings(settings: any) {
  const executableNames: Record<string, RegExp> = {
    ffmpegPath: /^ffmpeg(?:\.exe)?$/i, ffprobePath: /^ffprobe(?:\.exe)?$/i,
    whisperPath: /^(?:whisper-cli|main)(?:\.exe)?$/i,
    llamaPath: /^llama-cli(?:\.exe)?$/i,
    embeddingPath: /^llama-(?:embedding|embeddings)(?:\.exe)?$/i,
  }
  for (const [field, pattern] of Object.entries(executableNames)) {
    const value = settings?.[field]
    if (value && (!path.isAbsolute(value) || !fsSync.existsSync(value) || !pattern.test(path.basename(value)))) throw new Error(`${field} 不是受支持的本地运行引擎`)
  }
  if (settings?.whisperModelPath && (!path.isAbsolute(settings.whisperModelPath) || !fsSync.existsSync(settings.whisperModelPath) || path.extname(settings.whisperModelPath).toLowerCase() !== '.bin')) throw new Error('Whisper 模型必须是存在的本地 .bin 文件')
  if (settings?.llamaModelPath && (!path.isAbsolute(settings.llamaModelPath) || !fsSync.existsSync(settings.llamaModelPath) || path.extname(settings.llamaModelPath).toLowerCase() !== '.gguf')) throw new Error('导演模型必须是存在的本地 .gguf 文件')
  if (settings?.embeddingModelPath && (!path.isAbsolute(settings.embeddingModelPath) || !fsSync.existsSync(settings.embeddingModelPath) || path.extname(settings.embeddingModelPath).toLowerCase() !== '.gguf')) throw new Error('Embedding 模型必须是存在的本地 .gguf 文件')
  return settings
}

function resolveProgram(program: string) {
  if (!program) return ''
  if (path.isAbsolute(program) || program.includes(path.sep)) return fsSync.existsSync(program) ? program : ''
  const extension = process.platform === 'win32' && !program.toLowerCase().endsWith('.exe') ? '.exe' : ''
  for (const directory of (process.env.PATH || '').split(path.delimiter)) {
    const candidate = path.join(directory, `${program}${extension}`)
    if (fsSync.existsSync(candidate)) return candidate
  }
  return ''
}

function run(program: string, args: string[], options: { input?: string; timeout?: number } = {}) {
  return new Promise<string>((resolve, reject) => {
    const executable = resolveProgram(program)
    if (!executable) return reject(new Error(`找不到本地运行程序：${program || '未配置'}`))
    const child = spawn(executable, args, { windowsHide: true, shell: false })
    activeProcess = child
    const chunks: Buffer[] = []
    const errors: Buffer[] = []
    const limit = 16 * 1024 * 1024
    const timer = setTimeout(() => {
      child.kill('SIGKILL')
      reject(new Error('本地任务超时'))
    }, options.timeout ?? 30 * 60 * 1000)
    child.stdout.on('data', (chunk: Buffer) => {
      chunks.push(chunk)
      if (Buffer.concat(chunks).length > limit) child.kill('SIGKILL')
    })
    child.stderr.on('data', (chunk: Buffer) => {
      errors.push(chunk)
      const text = chunk.toString('utf8').trim()
      if (text) emit('log', text.slice(-2000))
    })
    child.on('error', (error: Error) => { clearTimeout(timer); activeProcess = null; reject(error) })
    child.on('close', (code: number | null) => {
      clearTimeout(timer); activeProcess = null
      if (cancelled) return reject(new Error('任务已取消'))
      const stdout = Buffer.concat(chunks).toString('utf8')
      if (code === 0) resolve(stdout)
      else reject(new Error(`本地程序退出码 ${code}：${Buffer.concat(errors).toString('utf8').slice(-3000)}`))
    })
    if (options.input) child.stdin.end(options.input)
    else child.stdin.end()
  })
}

function parseRate(rate: string | undefined) {
  if (!rate) return 25
  const [left, right] = rate.split('/').map(Number)
  const value = right ? left / right : left
  return Number.isFinite(value) && value > 0 ? value : 25
}

function fingerprint(filePath: string) {
  const stat = fsSync.statSync(filePath)
  return crypto.createHash('sha256').update(`${path.resolve(filePath)}|${stat.size}|${stat.mtimeMs}`).digest('hex')
}

function mediaKind(filePath: string) {
  const extension = path.extname(filePath).toLowerCase()
  if (['.jpg', '.jpeg', '.png', '.webp'].includes(extension)) return 'image'
  if (['.mp3', '.wav', '.aac', '.flac', '.m4a'].includes(extension)) return 'audio'
  return 'video'
}

function extractJson(text: string) {
  const start = text.indexOf('{')
  if (start < 0) throw new Error('模型没有返回 JSON')
  let depth = 0, quoted = false, escaped = false
  for (let index = start; index < text.length; index += 1) {
    const character = text[index]
    if (quoted) {
      if (escaped) escaped = false
      else if (character === '\\') escaped = true
      else if (character === '"') quoted = false
    } else if (character === '"') quoted = true
    else if (character === '{') depth += 1
    else if (character === '}' && --depth === 0) return JSON.parse(text.slice(start, index + 1))
  }
  throw new Error('模型 JSON 不完整')
}

async function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1440, height: 900, minWidth: 1080, minHeight: 680,
    backgroundColor: '#090b0e', title: 'ShotClip AI',
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      contextIsolation: true, nodeIntegration: false, sandbox: true,
    },
  })
  const devUrl = process.env.SHOTCLIP_DEV_URL
  if (devUrl) await mainWindow.loadURL(devUrl)
  else await mainWindow.loadFile(path.join(__dirname, '..', 'dist', 'index.html'))
}

app.whenReady().then(createWindow)
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit() })
app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow() })

ipcMain.handle('dialog:media', async () => {
  const result = await dialog.showOpenDialog(mainWindow!, { properties: ['openFile', 'multiSelections'], filters: [{ name: '媒体', extensions: ['mp4', 'mov', 'mkv', 'avi', 'webm', 'mp3', 'wav', 'aac', 'flac', 'm4a', 'jpg', 'jpeg', 'png', 'webp'] }] })
  return result.canceled ? [] : result.filePaths
})
ipcMain.handle('dialog:media-folder', async () => {
  const result = await dialog.showOpenDialog(mainWindow!, { properties: ['openDirectory'] })
  if (result.canceled) return []
  const supported = new Set(['.mp4', '.mov', '.mkv', '.avi', '.webm', '.mp3', '.wav', '.aac', '.flac', '.m4a', '.jpg', '.jpeg', '.png', '.webp'])
  const found: string[] = []
  async function walk(directory: string) {
    for (const entry of await fs.readdir(directory, { withFileTypes: true })) {
      if (cancelled) return
      const filePath = path.join(directory, entry.name)
      if (entry.isDirectory()) await walk(filePath)
      else if (entry.isFile() && supported.has(path.extname(entry.name).toLowerCase())) found.push(filePath)
    }
  }
  cancelled = false
  await walk(result.filePaths[0])
  return found
})
ipcMain.handle('dialog:file', async (_event: unknown, kind: string) => {
  const filters = kind === 'gguf' ? [{ name: 'GGUF 模型', extensions: ['gguf'] }] : kind === 'bin' ? [{ name: 'Whisper 模型', extensions: ['bin'] }] : [{ name: '运行程序', extensions: process.platform === 'win32' ? ['exe'] : ['*'] }]
  const result = await dialog.showOpenDialog(mainWindow!, { properties: ['openFile'], filters })
  return result.canceled ? '' : result.filePaths[0]
})
ipcMain.handle('dialog:directory', async () => {
  const result = await dialog.showOpenDialog(mainWindow!, { properties: ['openDirectory', 'createDirectory'] })
  return result.canceled ? '' : result.filePaths[0]
})
ipcMain.handle('settings:load', async () => {
  return loadModelSettings()
})
ipcMain.handle('settings:save', async (_event: unknown, settings: unknown) => {
  validateModelSettings(settings)
  await atomicWrite(path.join(app.getPath('userData'), 'model-settings.json'), JSON.stringify(settings, null, 2)); return true
})
ipcMain.handle('media:probe', async (_event: unknown, paths: string[]) => {
  const settings = await loadModelSettings()
  const ffprobe = settings.ffprobePath || 'ffprobe'
  const assets = []
  for (const filePath of paths) {
    emit('status', `读取素材：${path.basename(filePath)}`)
    const output = await run(ffprobe, ['-v', 'error', '-show_format', '-show_streams', '-of', 'json', filePath])
    const data = JSON.parse(output)
    const video = data.streams?.find((stream: any) => stream.codec_type === 'video')
    const hasAudio = Boolean(data.streams?.some((stream: any) => stream.codec_type === 'audio'))
    const id = crypto.randomUUID()
    assets.push({ id, name: path.basename(filePath), path: filePath, kind: mediaKind(filePath), duration: Number(data.format?.duration || (mediaKind(filePath) === 'image' ? 5 : 0)), width: Number(video?.width || 0), height: Number(video?.height || 0), fps: parseRate(video?.avg_frame_rate), hasAudio, status: hasAudio ? 'pending' : 'ready', progress: hasAudio ? 0 : 1, segments: [], fingerprint: fingerprint(filePath) })
  }
  return assets
})
ipcMain.handle('media:transcribe', async (_event: unknown, asset: any) => {
  cancelled = false
  const settings = await loadModelSettings()
  if (!asset.hasAudio) return { ...asset, status: 'ready', progress: 1 }
  const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'shotclip-'))
  const cacheDirectory = path.join(app.getPath('userData'), 'transcription-cache')
  const cachePath = path.join(cacheDirectory, `${asset.fingerprint}.json`)
  let cache: { completed: number; segments: any[] } = { completed: 0, segments: [] }
  try {
    const parsed = JSON.parse(await fs.readFile(cachePath, 'utf8'))
    if (Number.isFinite(parsed.completed) && parsed.completed >= 0 && Array.isArray(parsed.segments)) cache = parsed
  } catch { /* no reusable checkpoint */ }
  const segments = cache.segments.map((segment: any, index: number) => ({ ...segment, id: `${asset.id}:${Math.round(segment.start * 1000)}:${index}`, assetId: asset.id }))
  try {
    const chunkSize = 300
    for (let offset = cache.completed; offset < asset.duration; offset += chunkSize) {
      if (cancelled) throw new Error('任务已取消')
      const wave = path.join(temp, 'chunk.wav'), prefix = path.join(temp, 'transcript')
      emit('status', `转录 ${asset.name} · ${Math.round(offset)}/${Math.round(asset.duration)} 秒`, offset / asset.duration)
      await run(settings.ffmpegPath || 'ffmpeg', ['-nostdin', '-v', 'error', '-y', '-ss', String(offset), '-i', asset.path, '-t', String(Math.min(chunkSize, asset.duration - offset)), '-vn', '-ac', '1', '-ar', '16000', '-c:a', 'pcm_s16le', wave])
      await run(settings.whisperPath, ['-m', settings.whisperModelPath, '-f', wave, '-l', 'auto', '-osrt', '-of', prefix, '-pp', '-t', '8'])
      const srt = await fs.readFile(`${prefix}.srt`, 'utf8')
      const parsed = parseWhisperSrt(srt, asset.id, offset)
      segments.push(...parsed.segments)
      if (parsed.skipped > 0) emit('log', `Whisper 输出中已跳过 ${parsed.skipped} 条空白、零时长或格式异常字幕`)
      const completed = Math.min(asset.duration, offset + chunkSize)
      await atomicWrite(cachePath, JSON.stringify({ completed, segments }))
      await fs.rm(`${prefix}.srt`, { force: true })
    }
    emit('status', `${asset.name} 转录完成`, 1)
    return { ...asset, status: 'ready', progress: 1, segments }
  } finally { await fs.rm(temp, { recursive: true, force: true }) }
})
ipcMain.handle('ai:plan', async (_event: unknown, payload: any) => {
  cancelled = false
  const { brief, candidates, assets } = payload
  const settings = await loadModelSettings()
  const context = candidates.map((segment: any) => ({ id: segment.id, assetId: segment.assetId, start: segment.start, end: segment.end, text: segment.text }))
  const assetContext = assets.map((asset: any) => ({ id: asset.id, name: asset.name, duration: asset.duration }))
  const prompt = `你是离线纪录片剪辑导演。只能引用给定素材和证据，不能创造原话。用户要求：${brief}\n素材：${JSON.stringify(assetContext)}\n候选证据：${JSON.stringify(context)}\n返回严格 JSON：{\"version\":\"1.0\",\"title\":string,\"summary\":string,\"targetDuration\":number,\"clips\":[{\"id\":string,\"assetId\":string,\"sourceIn\":number,\"sourceOut\":number,\"timelineStart\":number,\"videoTrack\":1,\"audioTrack\":1,\"reason\":string,\"evidenceSegmentIds\":[string]}],\"warnings\":[string]}。片段在时间线上不得重叠，timelineStart 从 0 连续排列。`
  const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'shotclip-plan-'))
  const promptFile = path.join(temp, 'prompt.txt')
  try {
    await fs.writeFile(promptFile, prompt, 'utf8')
    emit('status', '本地导演模型正在生成粗剪方案…')
    const outputSchema = JSON.stringify({ type: 'object', additionalProperties: false, required: ['version', 'title', 'summary', 'targetDuration', 'clips', 'warnings'], properties: { version: { const: '1.0' }, title: { type: 'string' }, summary: { type: 'string' }, targetDuration: { type: 'number', exclusiveMinimum: 0 }, warnings: { type: 'array', items: { type: 'string' } }, clips: { type: 'array', minItems: 1, items: { type: 'object', additionalProperties: false, required: ['id', 'assetId', 'sourceIn', 'sourceOut', 'timelineStart', 'videoTrack', 'audioTrack', 'reason', 'evidenceSegmentIds'], properties: { id: { type: 'string' }, assetId: { type: 'string' }, sourceIn: { type: 'number', minimum: 0 }, sourceOut: { type: 'number', exclusiveMinimum: 0 }, timelineStart: { type: 'number', minimum: 0 }, videoTrack: { type: 'integer', minimum: 1, maximum: 16 }, audioTrack: { type: 'integer', minimum: 1, maximum: 16 }, reason: { type: 'string' }, evidenceSegmentIds: { type: 'array', minItems: 1, items: { type: 'string' } } } } } } })
    const output = await run(settings.llamaPath, ['-m', settings.llamaModelPath, '-f', promptFile, '-n', '4096', '-c', String(settings.contextSize || 8192), '-ngl', String(settings.gpuLayers ?? 999), '--temp', '0.2', '--json-schema', outputSchema, '--no-display-prompt'])
    return extractJson(output)
  } finally { await fs.rm(temp, { recursive: true, force: true }) }
})
ipcMain.handle('ai:rank', async (_event: unknown, payload: any) => {
  const settings = await loadModelSettings()
  if (!settings.embeddingPath || !settings.embeddingModelPath) return null
  const candidates = Array.isArray(payload?.candidates) ? payload.candidates.slice(0, 80) : []
  if (!payload?.brief || !candidates.length) return null
  const separator = '<#shotclip-embedding-separator#>'
  const clean = (value: unknown) => String(value ?? '').replaceAll(separator, ' ').replace(/[\r\n]+/g, ' ')
  const input = [clean(payload.brief), ...candidates.map((item: any) => clean(item.text))].join(separator)
  emit('status', `Embedding 模型正在对 ${candidates.length} 段证据排序…`)
  const output = await run(settings.embeddingPath, ['-m', settings.embeddingModelPath, '-p', input, '--pooling', 'mean', '--embd-separator', separator, '--embd-normalize', '2', '--embd-output-format', 'array', '--n-gpu-layers', String(settings.gpuLayers ?? 999), '--log-disable'])
  const vectors = JSON.parse(output)
  if (!Array.isArray(vectors) || vectors.length !== candidates.length + 1) throw new Error('Embedding 输出数量与候选证据不一致')
  const query = vectors[0]
  const dot = (left: number[], right: number[]) => left.reduce((sum, value, index) => sum + value * (right[index] ?? 0), 0)
  return candidates.map((item: any, index: number) => ({ id: item.id, score: dot(query, vectors[index + 1]) }))
    .sort((left: any, right: any) => right.score - left.score).slice(0, 40).map((item: any) => item.id)
})
ipcMain.handle('project:save', async (_event: unknown, project: any, saveAs: boolean) => {
  let filePath = project.filePath
  if (!filePath || saveAs) {
    const result = await dialog.showSaveDialog(mainWindow!, { defaultPath: `${project.name || 'ShotClip工程'}.shotclip.json`, filters: [{ name: 'ShotClip 工程', extensions: ['json'] }] })
    if (result.canceled || !result.filePath) return null
    filePath = result.filePath
  }
  await atomicWrite(filePath, JSON.stringify({ ...project, filePath, updatedAt: new Date().toISOString() }, null, 2))
  return filePath
})
ipcMain.handle('project:open', async () => {
  const result = await dialog.showOpenDialog(mainWindow!, { properties: ['openFile'], filters: [{ name: 'ShotClip 工程', extensions: ['json'] }] })
  if (result.canceled) return null
  return JSON.parse(await fs.readFile(result.filePaths[0], 'utf8'))
})
ipcMain.handle('project:checkpoint', async (_event: unknown, project: unknown) => {
  await atomicWrite(path.join(app.getPath('userData'), 'recovery', 'latest.json'), JSON.stringify(project, null, 2)); return true
})
ipcMain.handle('project:recover', async () => {
  try { return JSON.parse(await fs.readFile(path.join(app.getPath('userData'), 'recovery', 'latest.json'), 'utf8')) }
  catch { return null }
})
ipcMain.handle('export:package', async (_event: unknown, payload: any) => {
  const result = await dialog.showOpenDialog(mainWindow!, { properties: ['openDirectory', 'createDirectory'] })
  if (result.canceled) return null
  const folder = path.join(result.filePaths[0], payload.folderName.replace(/[<>:"/\\|?*]/g, '_'))
  await fs.mkdir(path.join(folder, 'premiere'), { recursive: true })
  await fs.mkdir(path.join(folder, 'subtitles'), { recursive: true })
  await fs.mkdir(path.join(folder, 'reports'), { recursive: true })
  await fs.mkdir(path.join(folder, 'evidence'), { recursive: true })
  await Promise.all([
    atomicWrite(path.join(folder, 'premiere', 'rough-cut.xml'), payload.xml),
    atomicWrite(path.join(folder, 'subtitles', 'subtitles.srt'), payload.srt),
    atomicWrite(path.join(folder, 'reports', 'editor-report.md'), payload.report),
    atomicWrite(path.join(folder, 'evidence', 'evidence.json'), JSON.stringify(payload.evidence, null, 2)),
    atomicWrite(path.join(folder, 'shotclip-project.json'), JSON.stringify(payload.project, null, 2)),
  ])
  return folder
})
ipcMain.handle('task:cancel', () => { cancelled = true; activeProcess?.kill('SIGKILL'); return true })

process.on('uncaughtException', (error) => emit('error', safeError(error)))
process.on('unhandledRejection', (error) => emit('error', safeError(error)))
