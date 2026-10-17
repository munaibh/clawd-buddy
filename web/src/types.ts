export interface Variant {
  id: string
  label: string
}

export interface VersionEntry {
  version: string
  note: string
  date: string | null
}

export interface VersionNote {
  version: string
  note: string
}
