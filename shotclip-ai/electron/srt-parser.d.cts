type WhisperSegment = { id: string; assetId: string; start: number; end: number; text: string }
declare function parseWhisperSrt(text: string, assetId: string, offset: number): { segments: WhisperSegment[]; skipped: number }
export = { parseWhisperSrt }
