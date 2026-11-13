import { monaco } from "./models";
import { pathForModel, absolutePath } from "./models";
import { api } from "../ipc/api";
import type {
  LspCompletionItem,
  LspHover,
  LspMarkupContent,
  LspPosition,
  LspRange,
  LspSignatureHelp,
} from "../ipc/types";
import { useEditorStore } from "../stores/editor";
import { useWorkspaceStore } from "../stores/workspace";

let registered = false;

function toPosition(position: monaco.Position): LspPosition {
  return { line: position.lineNumber - 1, character: position.column - 1 };
}

function toRange(range: LspRange): monaco.Range {
  return new monaco.Range(
    range.start.line + 1,
    range.start.character + 1,
    range.end.line + 1,
    range.end.character + 1,
  );
}

function markupToString(value: LspMarkupContent | string): string {
  return typeof value === "string" ? value : value.value;
}

function hoverContents(hover: LspHover): string {
  const contents = hover.contents;
  if (typeof contents === "string") return contents;
  if (Array.isArray(contents)) {
    return contents
      .map((item) => (typeof item === "string" ? item : `\`\`\`${item.language}\n${item.value}\n\`\`\``))
      .join("\n\n---\n\n");
  }
  return contents.value;
}

const completionKind = (kind: number | undefined): monaco.languages.CompletionItemKind => {
  switch (kind) {
    case 2:
      return monaco.languages.CompletionItemKind.Method;
    case 3:
      return monaco.languages.CompletionItemKind.Function;
    case 4:
      return monaco.languages.CompletionItemKind.Constructor;
    case 5:
      return monaco.languages.CompletionItemKind.Field;
    case 6:
      return monaco.languages.CompletionItemKind.Variable;
    case 7:
      return monaco.languages.CompletionItemKind.Class;
    case 8:
      return monaco.languages.CompletionItemKind.Interface;
    case 9:
      return monaco.languages.CompletionItemKind.Module;
    case 10:
      return monaco.languages.CompletionItemKind.Property;
    case 11:
      return monaco.languages.CompletionItemKind.Unit;
    case 12:
      return monaco.languages.CompletionItemKind.Value;
    case 13:
      return monaco.languages.CompletionItemKind.Enum;
    case 14:
      return monaco.languages.CompletionItemKind.Keyword;
    case 15:
      return monaco.languages.CompletionItemKind.Snippet;
    case 16:
      return monaco.languages.CompletionItemKind.Color;
    case 17:
      return monaco.languages.CompletionItemKind.File;
    case 18:
      return monaco.languages.CompletionItemKind.Reference;
    case 19:
      return monaco.languages.CompletionItemKind.Folder;
    case 20:
      return monaco.languages.CompletionItemKind.EnumMember;
    case 21:
      return monaco.languages.CompletionItemKind.Constant;
    case 22:
      return monaco.languages.CompletionItemKind.Struct;
    case 23:
      return monaco.languages.CompletionItemKind.Event;
    case 24:
      return monaco.languages.CompletionItemKind.Operator;
    case 25:
      return monaco.languages.CompletionItemKind.TypeParameter;
    default:
      return monaco.languages.CompletionItemKind.Text;
  }
};

function completionItem(
  item: LspCompletionItem,
  wordRange: monaco.Range,
): monaco.languages.CompletionItem {
  const label = typeof item.label === "string" ? item.label : item.label.label;
  const detail =
    item.detail ??
    (typeof item.label === "object" ? item.label.detail : undefined) ??
    item.labelDetails?.detail;
  const documentation =
    item.documentation === undefined ? undefined : markupToString(item.documentation);
  const range = item.textEdit ? toRange(item.textEdit.range) : wordRange;
  const insertText = item.textEdit
    ? item.textEdit.newText
    : (item.insertText ?? label);
  const suggestion: monaco.languages.CompletionItem = {
    label,
    kind: completionKind(item.kind),
    insertText,
    range,
    ...(detail ? { detail } : {}),
    ...(documentation ? { documentation } : {}),
    ...(item.sortText ? { sortText: item.sortText } : {}),
    ...(item.filterText ? { filterText: item.filterText } : {}),
    ...(item.deprecated ? { tags: [monaco.languages.CompletionItemTag.Deprecated] } : {}),
    ...(item.preselect ? { preselect: true } : {}),
  };
  if (item.insertTextFormat === 2) {
    suggestion.insertTextRules =
      monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet;
  }
  if (item.additionalTextEdits) {
    suggestion.additionalTextEdits = item.additionalTextEdits.map((edit) => ({
      range: toRange(edit.range),
      text: edit.newText,
    }));
  }
  return suggestion;
}

function signatureHelp(result: LspSignatureHelp): monaco.languages.SignatureHelpResult {
  return {
    value: {
      signatures: result.signatures.map((signature) => ({
        label: signature.label,
        documentation:
          signature.documentation === undefined
            ? undefined
            : markupToString(signature.documentation),
        parameters:
          signature.parameters?.map((parameter) => ({
            label: parameter.label,
            documentation:
              parameter.documentation === undefined
                ? undefined
                : markupToString(parameter.documentation),
          })) ?? [],
      })),
      activeSignature: result.activeSignature ?? 0,
      activeParameter: result.activeParameter ?? 0,
    },
    dispose: () => undefined,
  };
}

export function registerProviders(): void {
  if (registered) return;
  registered = true;

  monaco.languages.registerHoverProvider("cpp", {
    provideHover: async (model, position) => {
      const path = pathForModel(model);
      if (!path) return null;
      try {
        const hover = await api.lspHover(path, toPosition(position));
        if (!hover) return null;
        return {
          range: hover.range ? toRange(hover.range) : undefined,
          contents: [{ value: hoverContents(hover) }],
        };
      } catch {
        return null;
      }
    },
  });

  monaco.languages.registerCompletionItemProvider("cpp", {
    triggerCharacters: [".", ">", ":", "::", '"', "/", "<", "("],
    provideCompletionItems: async (model, position, _context, token) => {
      const path = pathForModel(model);
      if (!path) return { suggestions: [] };
      const word = model.getWordUntilPosition(position);
      const wordRange = new monaco.Range(
        position.lineNumber,
        word.startColumn,
        position.lineNumber,
        word.endColumn,
      );
      try {
        const response = await api.lspCompletion(path, toPosition(position));
        if (token.isCancellationRequested) return { suggestions: [] };
        if (!response) return { suggestions: [] };
        const items = Array.isArray(response) ? response : response.items;
        return {
          suggestions: items.map((item) => completionItem(item, wordRange)),
          incomplete: Array.isArray(response) ? false : response.isIncomplete,
        };
      } catch {
        return { suggestions: [] };
      }
    },
  });

  monaco.languages.registerDefinitionProvider("cpp", {
    provideDefinition: async (model, position) => {
      const path = pathForModel(model);
      if (!path) return null;
      try {
        const result = await api.lspDefinition(path, toPosition(position));
        const root = useWorkspaceStore.getState().root;
        if (!root) return null;
        const locations: monaco.languages.Location[] = [];
        for (const location of result.locations) {
          const opened = await useEditorStore.getState().openFile(location.path, {
            line: location.range.start.line + 1,
            character: location.range.start.character + 1,
            reveal: false,
          });
          if (!opened) continue;
          locations.push({
            uri: monaco.Uri.file(absolutePath(root, location.path)),
            range: toRange(location.range),
          });
        }
        return locations;
      } catch {
        return null;
      }
    },
  });

  monaco.languages.registerSignatureHelpProvider("cpp", {
    signatureHelpTriggerCharacters: ["(", ","],
    signatureHelpRetriggerCharacters: [")"],
    provideSignatureHelp: async (model, position) => {
      const path = pathForModel(model);
      if (!path) return null;
      try {
        const result = await api.lspSignatureHelp(path, toPosition(position));
        return result ? signatureHelp(result) : null;
      } catch {
        return null;
      }
    },
  });
}
