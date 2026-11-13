import EditorWorker from "monaco-editor/esm/vs/editor/editor.worker?worker&inline";
import * as monaco from "monaco-editor/esm/vs/editor/edcore.main";
import "monaco-editor/min/vs/editor/editor.main.css";
import "monaco-editor/esm/vs/basic-languages/cpp/cpp.contribution";
import "monaco-editor/esm/vs/basic-languages/python/python.contribution";
import "monaco-editor/esm/vs/basic-languages/rust/rust.contribution";
import "monaco-editor/esm/vs/basic-languages/markdown/markdown.contribution";
import "monaco-editor/esm/vs/basic-languages/yaml/yaml.contribution";
import "monaco-editor/esm/vs/basic-languages/shell/shell.contribution";

// A single Monaco instance owns all models. Tabs swap models instead of
// creating editors, which keeps memory bounded and undo history per file.
(self as unknown as { MonacoEnvironment: { getWorker: () => Worker } }).MonacoEnvironment = {
  getWorker: () => new EditorWorker(),
};

const models = new Map<string, monaco.editor.ITextModel>();
const pathsByModel = new Map<monaco.editor.ITextModel, string>();
let programmaticDepth = 0;

export function languageIdForCore(language: string): string {
  switch (language) {
    case "cpp":
    case "c":
      return "cpp";
    case "json":
      return "json";
    case "markdown":
      return "markdown";
    case "python":
      return "python";
    case "rust":
      return "rust";
    case "javascript":
      return "javascript";
    case "typescript":
      return "typescript";
    case "yaml":
      return "yaml";
    case "shell":
      return "shell";
    case "cmake":
      return "cmake";
    default:
      return "plaintext";
  }
}

export function absolutePath(root: string, path: string): string {
  if (!root) return path;
  const normalizedRoot = root.endsWith("/") ? root.slice(0, -1) : root;
  return `${normalizedRoot}/${path}`;
}

export function modelUri(root: string, path: string): monaco.Uri {
  return monaco.Uri.file(absolutePath(root, path));
}

export function getModel(path: string): monaco.editor.ITextModel | undefined {
  return models.get(path);
}

export function pathForModel(model: monaco.editor.ITextModel): string | undefined {
  return pathsByModel.get(model);
}

function detectEol(content: string): monaco.editor.EndOfLineSequence {
  const crlf = (content.match(/\r\n/g) ?? []).length;
  const lf = (content.match(/(?<!\r)\n/g) ?? []).length;
  return crlf > lf
    ? monaco.editor.EndOfLineSequence.CRLF
    : monaco.editor.EndOfLineSequence.LF;
}

export function getOrCreateModel(
  path: string,
  root: string,
  content: string,
  language: string,
): monaco.editor.ITextModel {
  const existing = models.get(path);
  if (existing) return existing;
  const uri = modelUri(root, path);
  const already = monaco.editor.getModel(uri);
  if (already) {
    models.set(path, already);
    pathsByModel.set(already, path);
    return already;
  }
  const model = monaco.editor.createModel(
    content,
    languageIdForCore(language),
    uri,
  );
  const eol = detectEol(content);
  if (eol === monaco.editor.EndOfLineSequence.CRLF) {
    model.pushEOL(eol);
  }
  models.set(path, model);
  pathsByModel.set(model, path);
  return model;
}

export function disposeModel(path: string): void {
  const model = models.get(path);
  if (!model) return;
  pathsByModel.delete(model);
  models.delete(path);
  model.dispose();
}

export function disposeAllModels(): void {
  for (const model of models.values()) model.dispose();
  models.clear();
  pathsByModel.clear();
}

export function beginProgrammaticChange(): void {
  programmaticDepth += 1;
}

export function endProgrammaticChange(): void {
  programmaticDepth = Math.max(0, programmaticDepth - 1);
}

export function isProgrammaticChange(): boolean {
  return programmaticDepth > 0;
}

export { monaco };
