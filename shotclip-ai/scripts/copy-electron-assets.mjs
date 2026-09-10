import { copyFile, mkdir } from 'node:fs/promises'

await mkdir('dist-electron', { recursive: true })
await copyFile('electron/srt-parser.cjs', 'dist-electron/srt-parser.cjs')
