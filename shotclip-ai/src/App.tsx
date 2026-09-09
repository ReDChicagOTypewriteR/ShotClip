import { useEffect, useMemo, useState } from 'react'
import { Bot, CheckCircle2, ChevronRight, CircleDot, FileOutput, FolderOpen, Import, Library, LoaderCircle, Play, Save, Search, Settings, Sparkles, Square, Terminal, XCircle } from 'lucide-react'
import { useAppStore } from './store'
import { emptySettings, projectSchema, validatePlan, type Asset, type ModelSettings } from './domain'
import { compilePremiereXml, compileSrt } from './workflow/premiereXml'
import { makeFallbackPlan, retrieveSegments } from './workflow/retrieval'

const formatTime = (seconds: number) => {
  const value = Math.max(0, Math.floor(seconds))
  return `${String(Math.floor(value / 3600)).padStart(2, '0')}:${String(Math.floor(value / 60) % 60).padStart(2, '0')}:${String(value % 60).padStart(2, '0')}`
}

function Logo() {
  return <div className="brand"><span className="brand-mark">S<span>C</span></span><div><strong>SHOTCLIP</strong><small>OFFLINE AI EDITORIAL</small></div></div>
}

function ModelDialog({ close }: { close(): void }) {
  const current = useAppStore((state) => state.settings)
  const setSettings = useAppStore((state) => state.setSettings)
  const [settings, setLocal] = useState(current)
  const choose = async (field: keyof ModelSettings, kind: 'bin' | 'gguf' | 'exe') => {
    const file = await window.shotclip.selectFile(kind)
    if (file) setLocal({ ...settings, [field]: file })
  }
  const rows: Array<[keyof ModelSettings, string, 'bin' | 'gguf' | 'exe']> = [
    ['ffmpegPath', 'FFmpeg', 'exe'], ['ffprobePath', 'FFprobe', 'exe'],
    ['whisperPath', 'Whisper 运行引擎', 'exe'], ['whisperModelPath', 'Whisper 模型', 'bin'],
    ['llamaPath', 'llama.cpp 运行引擎', 'exe'], ['llamaModelPath', '导演 GGUF 模型', 'gguf'],
    ['embeddingPath', 'Embedding 运行引擎（可选）', 'exe'], ['embeddingModelPath', 'Embedding 模型（可选）', 'gguf'],
  ]
  return <div className="modal-backdrop"><section className="modal">
    <header><div><small>LOCAL RUNTIME</small><h2>本地模型与运行引擎</h2></div><button className="icon-button" onClick={close}>×</button></header>
    <p className="muted">所有路径只保存在本机。模型不会复制进安装目录，也不会发往网络。</p>
    <div className="model-grid">{rows.map(([field, label, kind]) => <label key={field}><span>{label}</span><div><input value={String(settings[field])} readOnly placeholder="尚未选择"/><button onClick={() => choose(field, kind)}>选择</button></div></label>)}</div>
    <div className="number-row"><label>上下文长度<input type="number" value={settings.contextSize} onChange={(event) => setLocal({ ...settings, contextSize: Number(event.target.value) })}/></label><label>GPU 层数<input type="number" value={settings.gpuLayers} onChange={(event) => setLocal({ ...settings, gpuLayers: Number(event.target.value) })}/></label></div>
    <footer><button className="secondary" onClick={close}>取消</button><button className="primary" onClick={async () => { await window.shotclip.saveSettings(settings); setSettings(settings); close() }}>保存本地配置</button></footer>
  </section></div>
}

function AssetList({ assets, selected, select }: { assets: Asset[]; selected?: string; select(id: string): void }) {
  return <div className="asset-list">{assets.length === 0 ? <div className="empty"><Library/><b>还没有素材</b><span>导入长视频或音频后，ShotClip 会在本地建立文稿索引。</span></div> : assets.map((asset) => <button key={asset.id} className={`asset-row ${selected === asset.id ? 'selected' : ''}`} onClick={() => select(asset.id)}>
    <span className={`status-dot ${asset.status}`}/><span className="asset-copy"><b>{asset.name}</b><small>{asset.kind.toUpperCase()} · {formatTime(asset.duration)} · {asset.width ? `${asset.width}×${asset.height}` : '音频'} · {asset.segments.length} 段文稿</small></span><span className="state">{asset.status === 'ready' ? '已就绪' : asset.status === 'failed' ? '失败' : asset.status === 'analyzing' ? '分析中' : '待分析'}</span>
  </button>)}</div>
}

export default function App() {
  const store = useAppStore()
  const [tab, setTab] = useState<'library' | 'transcript' | 'director' | 'review'>('library')
  const [showModels, setShowModels] = useState(false)
  const [showConsole, setShowConsole] = useState(false)
  const [query, setQuery] = useState('')
  const [notice, setNotice] = useState('就绪')
  useEffect(() => {
    window.shotclip.loadSettings().then((loaded) => store.setSettings({ ...emptySettings, ...loaded }))
    window.shotclip.recoverProject().then((project) => { const parsed = projectSchema.safeParse(project); if (parsed.success && parsed.data.assets.length) { store.setProject(parsed.data); setNotice('已恢复上次自动保存的工作区') } })
    return window.shotclip.onEvent((event) => { store.addLog(event); setNotice(event.message); if (event.progress !== undefined) store.setTaskProgress(event.progress) })
  }, []) // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => {
    const timer = window.setTimeout(() => { window.shotclip.checkpointProject(store.project).catch(() => undefined) }, 800)
    return () => window.clearTimeout(timer)
  }, [store.project])
  const selected = store.project.assets.find((asset) => asset.id === store.selectedAssetId)
  const allSegments = useMemo(() => store.project.assets.flatMap((asset) => asset.segments), [store.project.assets])
  const transcriptHits = useMemo(() => query.trim() ? allSegments.filter((segment) => segment.text.toLowerCase().includes(query.trim().toLowerCase())) : allSegments, [allSegments, query])
  const run = async (action: () => Promise<void>) => {
    store.setBusy(true)
    try { await action() } catch (error) { const message = error instanceof Error ? error.message : String(error); setNotice(message); store.addLog({ type: 'error', message, at: new Date().toISOString() }) } finally { store.setBusy(false) }
  }
  const importMedia = () => run(async () => {
    const paths = await window.shotclip.selectMedia(); if (!paths.length) return
    const known = new Set(store.project.assets.map((asset) => asset.path))
    const assets = await window.shotclip.probe(paths.filter((item) => !known.has(item)))
    store.addAssets(assets); setNotice(`已导入 ${assets.length} 个素材`)
  })
  const importFolder = () => run(async () => {
    const paths = await window.shotclip.selectMediaFolder(); if (!paths.length) return
    const known = new Set(store.project.assets.map((asset) => asset.path))
    const assets = await window.shotclip.probe(paths.filter((item) => !known.has(item)))
    store.addAssets(assets); setNotice(`已从文件夹导入 ${assets.length} 个素材`)
  })
  const analyze = () => run(async () => {
    const pending = store.project.assets.filter((asset) => asset.hasAudio && asset.status !== 'ready')
    if (!store.settings.whisperPath || !store.settings.whisperModelPath) throw new Error('请先配置 Whisper 运行引擎和模型')
    for (const asset of pending) {
      store.replaceAsset({ ...asset, status: 'analyzing' })
      try { store.replaceAsset(await window.shotclip.transcribe(asset, store.settings)) }
      catch (error) { store.replaceAsset({ ...asset, status: 'failed', error: error instanceof Error ? error.message : String(error) }) }
    }
    setNotice('素材分析队列完成')
  })
  const generate = () => run(async () => {
    if (!store.project.brief.trim()) throw new Error('请先描述你希望生成的成片')
    if (!allSegments.length) throw new Error('没有可用文稿，请先转录素材')
    let candidates = retrieveSegments(store.project.assets, store.project.brief)
    if (store.settings.embeddingPath && store.settings.embeddingModelPath) {
      try {
        const rankedIds = await window.shotclip.rankSegments({ brief: store.project.brief, candidates })
        if (rankedIds) { const byId = new Map(candidates.map((item) => [item.id, item])); candidates = rankedIds.flatMap((id) => byId.get(id) ? [byId.get(id)!] : []) }
      } catch (error) {
        store.addLog({ type: 'error', message: `Embedding 排序失败，继续使用关键词候选：${error instanceof Error ? error.message : String(error)}`, at: new Date().toISOString() })
      }
    }
    let raw: unknown
    if (store.settings.llamaPath && store.settings.llamaModelPath) raw = await window.shotclip.generatePlan({ brief: store.project.brief, candidates, assets: store.project.assets })
    else raw = makeFallbackPlan(store.project.assets, store.project.brief)
    const plan = validatePlan(raw, store.project.assets)
    store.setPlan(plan); setTab('review'); setNotice(`方案已生成：${plan.clips.length} 个片段，等待人工确认`)
  })
  const save = (saveAs = false) => run(async () => {
    const filePath = await window.shotclip.saveProject(store.project, saveAs)
    if (filePath) { store.setProject({ ...store.project, filePath }); setNotice(`工程已保存：${filePath}`) }
  })
  const exportProject = () => run(async () => {
    if (!store.project.plan) throw new Error('请先生成并检查粗剪方案')
    const xml = compilePremiereXml(store.project, store.project.plan)
    const srt = compileSrt(store.project, store.project.plan)
    const evidenceIds = new Set(store.project.plan.clips.flatMap((clip) => clip.evidenceSegmentIds))
    const evidence = allSegments.filter((segment) => evidenceIds.has(segment.id))
    const report = `# ${store.project.plan.title}\n\n${store.project.plan.summary}\n\n## 用户要求\n\n${store.project.brief}\n\n## 人工检查\n\n- 在 Premiere 中检查所有素材重新链接和帧率。\n- 核对敏感表述上下文，AI 选择不等于事实审核。\n- 检查字幕、音频、调色和转场。\n\n${store.project.plan.warnings.map((item) => `- ${item}`).join('\n')}\n`
    const folder = await window.shotclip.exportPackage({ folderName: store.project.plan.title, xml, srt, report, evidence, project: store.project })
    if (folder) setNotice(`Premiere 工程包已输出：${folder}`)
  })
  const tabs = [{ id: 'library', icon: Library, label: '素材库' }, { id: 'transcript', icon: Search, label: '文稿中心' }, { id: 'director', icon: Bot, label: 'AI 导演' }, { id: 'review', icon: CircleDot, label: '粗剪审查' }] as const
  return <main className="app-shell">
    <header className="topbar"><Logo/><nav>{tabs.map((item) => <button key={item.id} className={tab === item.id ? 'active' : ''} onClick={() => setTab(item.id)}><item.icon size={16}/>{item.label}</button>)}</nav><div className="top-actions"><button onClick={() => save()}><Save size={16}/>保存</button><button onClick={async () => { const raw = await window.shotclip.openProject(); const parsed = projectSchema.safeParse(raw); if (parsed.success) store.setProject(parsed.data); else if (raw) setNotice('工程文件格式无效，未打开') }}><FolderOpen size={16}/>打开</button><button onClick={() => setShowModels(true)}><Settings size={16}/>模型</button><button className="export" disabled={!store.project.plan} onClick={exportProject}><FileOutput size={16}/>导出 PR 工程</button></div></header>
    <section className="workspace">
      <aside className="project-rail"><small>PROJECT</small><input className="project-name" value={store.project.name} onChange={(event) => store.setProject({ ...store.project, name: event.target.value })}/><div className="metrics"><div><b>{store.project.assets.length}</b><span>素材</span></div><div><b>{allSegments.length}</b><span>文稿段</span></div><div><b>{formatTime(store.project.assets.reduce((sum, asset) => sum + asset.duration, 0))}</b><span>总时长</span></div></div><div className="pipeline"><small>WORKFLOW</small>{['素材入库', '语音转录', '内容检索', '导演规划', '证据校验', 'PR 工程'].map((label, index) => <div key={label} className={(index === 0 && store.project.assets.length) || (index === 1 && allSegments.length) || (index >= 2 && store.project.plan) ? 'done' : ''}><span>{index + 1}</span>{label}</div>)}</div><button className="console-toggle" onClick={() => setShowConsole(!showConsole)}><Terminal size={15}/>本地运行日志</button></aside>
      <section className="content">
        {tab === 'library' && <><div className="page-title"><div><small>01 / INGEST</small><h1>把长素材变成可检索的内容库</h1><p>所有分析在本机进行，原始视频不会被上传或复制。</p></div><div><button className="secondary" disabled={store.busy} onClick={importFolder}><FolderOpen size={16}/>导入文件夹</button><button className="secondary" disabled={store.busy} onClick={importMedia}><Import size={16}/>导入素材</button><button className="primary" disabled={store.busy || !store.project.assets.length} onClick={analyze}>{store.busy ? <LoaderCircle className="spin" size={16}/> : <Play size={16}/>}开始分析</button></div></div><AssetList assets={store.project.assets} selected={store.selectedAssetId} select={store.setSelectedAsset}/>{selected && <div className="detail-card"><b>{selected.name}</b><span>{selected.path}</span><span>{selected.error || `${selected.segments.length} 个带时间码的文稿片段`}</span></div>}</>}
        {tab === 'transcript' && <><div className="page-title"><div><small>02 / TRANSCRIPT</small><h1>文稿中心</h1><p>搜索原话并始终保留素材与时间码证据。</p></div></div><div className="search-box"><Search size={18}/><input value={query} onChange={(event) => setQuery(event.target.value)} placeholder="搜索全部素材文稿…"/><span>{transcriptHits.length} 条</span></div><div className="transcript-list">{transcriptHits.slice(0, 500).map((segment) => { const asset = store.project.assets.find((item) => item.id === segment.assetId); return <article key={segment.id}><time>{formatTime(segment.start)} — {formatTime(segment.end)}</time><p>{segment.text}</p><small>{asset?.name}</small></article> })}{!transcriptHits.length && <div className="empty"><Search/><b>暂无文稿</b><span>完成素材分析后，可在这里搜索原话。</span></div>}</div></>}
        {tab === 'director' && <><div className="page-title"><div><small>03 / DIRECTOR</small><h1>描述你想要的成片</h1><p>AI 先生成可审查方案，不会直接修改素材或 Premiere 工程。</p></div></div><div className="brief-card"><Sparkles size={22}/><textarea value={store.project.brief} onChange={(event) => store.setBrief(event.target.value)} placeholder="例如：从这些监狱现身说法中制作一条 10 分钟警示教育片，重点讲犯罪动机、家庭影响、服刑反思和对年轻人的劝告。开头要有冲击力，删除重复表达，但不得改变受访者原意。"/><div className="brief-footer"><span>{store.settings.llamaModelPath ? '本地 LLM 已配置' : '未配置 LLM，将使用关键词演示模式'}</span><button className="primary" disabled={store.busy} onClick={generate}>{store.busy ? <LoaderCircle className="spin" size={16}/> : <Bot size={16}/>}生成粗剪方案</button></div></div></>}
        {tab === 'review' && <><div className="page-title"><div><small>04 / REVIEW</small><h1>粗剪审查</h1><p>核对每个片段的原始证据，再输出 Premiere XML。</p></div><button className="primary" disabled={!store.project.plan} onClick={exportProject}><FileOutput size={16}/>导出工程包</button></div>{store.project.plan ? <><div className="plan-summary"><CheckCircle2/><div><b>{store.project.plan.title}</b><p>{store.project.plan.summary}</p></div><strong>{store.project.plan.clips.length} CLIPS</strong></div><div className="clip-list">{store.project.plan.clips.map((clip, index) => { const asset = store.project.assets.find((item) => item.id === clip.assetId); const evidence = allSegments.filter((item) => clip.evidenceSegmentIds.includes(item.id)); return <article key={clip.id}><span className="clip-index">{String(index + 1).padStart(2, '0')}</span><div className="clip-main"><header><b>{asset?.name}</b><time>{formatTime(clip.sourceIn)} → {formatTime(clip.sourceOut)}</time></header><p>{evidence.map((item) => item.text).join(' ')}</p><small>{clip.reason}</small></div><ChevronRight/></article> })}</div></> : <div className="empty"><CircleDot/><b>还没有粗剪方案</b><span>前往 AI 导演，描述成片要求后生成方案。</span></div>}</>}
      </section>
    </section>
    <footer className="statusbar"><span className={store.busy ? 'working' : ''}>{store.busy ? <LoaderCircle className="spin" size={13}/> : <CheckCircle2 size={13}/>} {notice}</span>{store.busy && <><div className="progress"><i style={{ width: `${(store.taskProgress ?? 0.15) * 100}%` }}/></div><button onClick={() => window.shotclip.cancelTask()}><Square size={12}/>停止</button></>}</footer>
    {showConsole && <aside className="console"><header><span><Terminal size={15}/>本地控制台</span><button onClick={() => setShowConsole(false)}>×</button></header>{store.logs.length ? store.logs.map((log, index) => <pre key={`${log.at}-${index}`} className={log.type}>{log.at.slice(11, 19)}  {log.message}</pre>) : <div className="console-empty">暂无日志</div>}</aside>}
    {showModels && <ModelDialog close={() => setShowModels(false)}/>} 
  </main>
}
