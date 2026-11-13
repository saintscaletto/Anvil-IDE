import type { monaco } from "./models";

let activeEditor: monaco.editor.IStandaloneCodeEditor | null = null;

export function setActiveEditor(editor: monaco.editor.IStandaloneCodeEditor | null): void {
  activeEditor = editor;
}

export function getActiveEditor(): monaco.editor.IStandaloneCodeEditor | null {
  return activeEditor;
}
