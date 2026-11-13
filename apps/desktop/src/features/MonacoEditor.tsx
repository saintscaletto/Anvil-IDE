import { useCallback, useEffect, useRef } from "react";
import { monaco, getModel, pathForModel, isProgrammaticChange } from "../editor/models";
import { ensureTheme, editorOptions, applyEditorOptions } from "../editor/theme";
import { registerProviders } from "../editor/providers";
import { setActiveEditor } from "../editor/instance";
import { computeTextChange } from "../editor/textSync";
import { nextDocVersion, resetDocVersion } from "../editor/docVersions";
import { useEditorStore } from "../stores/editor";
import { useSettingsStore } from "../stores/settings";
import { useLspStore } from "../stores/lsp";
import { api } from "../ipc/api";

interface Props {
  onSave?: () => void;
  onSaveAll?: () => void;
  onClose?: () => void;
}

export function MonacoEditor({ onSave, onSaveAll, onClose }: Props) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const editorRef = useRef<monaco.editor.IStandaloneCodeEditor | null>(null);
  const disposables = useRef<Array<{ dispose: () => void }>>([]);
  const previousPath = useRef<string | null>(null);
  const viewStates = useRef(new Map<string, monaco.editor.ICodeEditorViewState | null>());
  const timers = useRef(new Map<string, number>());
  const activePath = useEditorStore((state) => state.activePath);
  const revealTarget = useEditorStore((state) => state.revealTarget);
  const settings = useSettingsStore((state) => state.values);
  const saveRef = useRef(onSave);
  const saveAllRef = useRef(onSaveAll);
  const closeRef = useRef(onClose);
  saveRef.current = onSave;
  saveAllRef.current = onSaveAll;
  closeRef.current = onClose;

  const ensureEditor = useCallback(() => {
    if (editorRef.current) return editorRef.current;
    if (!containerRef.current) return null;
    ensureTheme();
    registerProviders();
    const editor = monaco.editor.create(containerRef.current, {
      ...editorOptions(useSettingsStore.getState().values),
      model: null,
    });
    editorRef.current = editor;
    setActiveEditor(editor);

    disposables.current.push(
      editor.onDidChangeModelContent(() => {
        const model = editor.getModel();
        if (!model) return;
        const path = pathForModel(model);
        if (!path) return;
        if (isProgrammaticChange()) return;
        useEditorStore.getState().markDirty(path);
        if (model.getLanguageId() !== "cpp") return;

        const existing = timers.current.get(path);
        if (existing !== undefined) window.clearTimeout(existing);
        const timer = window.setTimeout(() => {
          timers.current.delete(path);
          const currentModel = getModel(path);
          if (!currentModel) return;
          const change = computeTextChange(path, currentModel.getValue());
          if (!change) return;
          const version = nextDocVersion(path);
          void api
            .lspDidChange(path, version, { changes: [change] })
            .catch((error: unknown) => {
              const message = error instanceof Error ? error.message : "";
              if (message.includes("not open")) {
                resetDocVersion(path, 1);
                const text = currentModel.getValue();
                const language =
                  useEditorStore
                    .getState()
                    .tabs.find((tab) => tab.path === path)?.language ?? "cpp";
                void api
                  .lspDidOpen(path, language === "c" ? "c" : "cpp", text, 1)
                  .catch((openError: unknown) => {
                    useLspStore.getState().handleFailure(openError);
                  });
              } else {
                useLspStore.getState().handleFailure(error);
              }
            });
        }, 55);
        timers.current.set(path, timer);
      }),
    );

    disposables.current.push(
      editor.onDidChangeCursorPosition((event) => {
        useEditorStore.getState().setCursor(event.position.lineNumber, event.position.column);
      }),
    );

    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => {
      saveRef.current?.();
    });
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyMod.Shift | monaco.KeyCode.KeyS, () => {
      saveAllRef.current?.();
    });
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyW, () => {
      closeRef.current?.();
    });
    return editor;
  }, []);

  useEffect(() => {
    const editor = ensureEditor();
    if (!editor) return;
    if (previousPath.current !== null) {
      viewStates.current.set(previousPath.current, editor.saveViewState());
    }
    const model = activePath ? getModel(activePath) : null;
    editor.setModel(model ?? null);
    previousPath.current = activePath;
    if (!model) {
      useEditorStore.getState().setCursor(1, 1);
      return;
    }
    const savedView = activePath ? viewStates.current.get(activePath) : null;
    if (savedView) {
      editor.restoreViewState(savedView);
    } else {
      editor.setPosition({ lineNumber: 1, column: 1 });
      editor.revealLine(1);
    }
    useEditorStore.getState().setEol(model.getEOL() === "\r\n" ? "CRLF" : "LF");
    const position = editor.getPosition();
    if (position) useEditorStore.getState().setCursor(position.lineNumber, position.column);
    editor.focus();
  }, [activePath, ensureEditor]);

  useEffect(() => {
    if (!activePath || !revealTarget || revealTarget.path !== activePath) return;
    const editor = editorRef.current;
    if (!editor) return;
    editor.setPosition({
      lineNumber: revealTarget.line,
      column: revealTarget.character,
    });
    editor.revealPositionInCenter({
      lineNumber: revealTarget.line,
      column: revealTarget.character,
    });
    editor.focus();
    useEditorStore.getState().clearReveal();
  }, [activePath, revealTarget]);

  useEffect(() => {
    const editor = editorRef.current;
    if (editor) applyEditorOptions(editor, settings);
  }, [settings]);

  useEffect(() => {
    return () => {
      for (const timer of timers.current.values()) window.clearTimeout(timer);
      timers.current.clear();
      for (const disposable of disposables.current) disposable.dispose();
      disposables.current = [];
      setActiveEditor(null);
      editorRef.current?.dispose();
      editorRef.current = null;
    };
  }, []);

  return <div className="editor-container" ref={containerRef} />;
}
