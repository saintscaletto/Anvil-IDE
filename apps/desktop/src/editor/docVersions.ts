const versions = new Map<string, number>();

export function resetDocVersion(path: string, version = 1): void {
  versions.set(path, version);
}

export function nextDocVersion(path: string): number {
  const next = (versions.get(path) ?? 1) + 1;
  versions.set(path, next);
  return next;
}

export function currentDocVersion(path: string): number {
  return versions.get(path) ?? 1;
}

export function forgetDocVersion(path: string): void {
  versions.delete(path);
}
