import { monaco } from "./models";
import { getModel } from "./models";
import type { LspDiagnostic } from "../ipc/types";

const OWNER = "clangd";

export function applyDiagnostics(path: string, diagnostics: LspDiagnostic[]): void {
  const model = getModel(path);
  if (!model) return;
  const markers: monaco.editor.IMarkerData[] = diagnostics.map((diagnostic) => {
    const startLine = Math.max(1, diagnostic.range.start.line + 1);
    const startColumn = Math.max(1, diagnostic.range.start.character + 1);
    const endLine = Math.max(startLine, diagnostic.range.end.line + 1);
    const endColumn = Math.max(
      endLine === startLine ? startColumn : 1,
      diagnostic.range.end.character + 1,
    );
    const severity =
      diagnostic.severity === 1
        ? monaco.MarkerSeverity.Error
        : diagnostic.severity === 2
          ? monaco.MarkerSeverity.Warning
          : diagnostic.severity === 3
            ? monaco.MarkerSeverity.Info
            : monaco.MarkerSeverity.Hint;
    const marker: monaco.editor.IMarkerData = {
      severity,
      message: diagnostic.message,
      startLineNumber: startLine,
      startColumn,
      endLineNumber: endLine,
      endColumn,
    };
    if (diagnostic.source !== undefined) marker.source = diagnostic.source;
    if (typeof diagnostic.code === "string" || typeof diagnostic.code === "number") {
      marker.code = String(diagnostic.code);
    }
    return marker;
  });
  monaco.editor.setModelMarkers(model, OWNER, markers);
}

export function clearDiagnostics(path: string): void {
  const model = getModel(path);
  if (model) monaco.editor.setModelMarkers(model, OWNER, []);
}
