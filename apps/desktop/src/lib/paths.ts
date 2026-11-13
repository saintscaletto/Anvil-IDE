export function dirname(path: string): string {
  const index = path.lastIndexOf("/");
  return index < 0 ? "" : path.slice(0, index);
}

export function basename(path: string): string {
  const index = path.lastIndexOf("/");
  return index < 0 ? path : path.slice(index + 1);
}

export function joinPath(directory: string, name: string): string {
  return directory ? `${directory}/${name}` : name;
}

export function segments(path: string): string[] {
  return path.split("/").filter(Boolean);
}

export function isValidEntryName(name: string): boolean {
  if (!name || name === "." || name === "..") return false;
  if (name.length > 255) return false;
  if (name.includes("/") || name.includes("\\")) return false;
  for (const character of name) {
    if (character.charCodeAt(0) < 0x20) return false;
  }
  return true;
}

export function parentOfChange(path: string): string {
  return dirname(path);
}

export function isWithin(path: string, directory: string): boolean {
  if (directory === "") return true;
  return path === directory || path.startsWith(`${directory}/`);
}
