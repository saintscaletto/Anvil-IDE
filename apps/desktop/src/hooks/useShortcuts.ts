import { useEffect } from "react";
import {
  closeActive,
  findReferences,
  openCommandPalette,
  openFileDialog,
  openFolderDialog,
  openQuickOpen,
  openSettings,
  requestCloseApp,
  saveActive,
  saveAll,
  showActivity,
  showPanel,
  togglePanel,
  toggleSidebar,
  newFile,
} from "../actions";

export function useShortcuts(): void {
  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      const modifier = event.ctrlKey || event.metaKey;
      const key = event.key.toLowerCase();
      if (modifier && event.shiftKey && key === "p") {
        event.preventDefault();
        event.stopPropagation();
        openCommandPalette();
        return;
      }
      if (modifier && !event.shiftKey && key === "p") {
        event.preventDefault();
        event.stopPropagation();
        openQuickOpen();
        return;
      }
      if (modifier && event.shiftKey && key === "s") {
        event.preventDefault();
        event.stopPropagation();
        void saveAll();
        return;
      }
      if (modifier && !event.shiftKey && key === "s") {
        event.preventDefault();
        event.stopPropagation();
        void saveActive();
        return;
      }
      if (modifier && !event.shiftKey && key === "w") {
        event.preventDefault();
        event.stopPropagation();
        void closeActive();
        return;
      }
      if (modifier && !event.shiftKey && key === "b") {
        event.preventDefault();
        toggleSidebar();
        return;
      }
      if (modifier && !event.shiftKey && key === "j") {
        event.preventDefault();
        togglePanel();
        return;
      }
      if (modifier && !event.shiftKey && key === "o") {
        event.preventDefault();
        event.stopPropagation();
        void openFileDialog();
        return;
      }
      if (modifier && event.shiftKey && key === "o") {
        event.preventDefault();
        event.stopPropagation();
        void openFolderDialog();
        return;
      }
      if (modifier && !event.shiftKey && key === "n") {
        event.preventDefault();
        void newFile("");
        return;
      }
      if (modifier && key === ",") {
        event.preventDefault();
        openSettings();
        return;
      }
      if (modifier && !event.shiftKey && key === "q") {
        event.preventDefault();
        event.stopPropagation();
        void requestCloseApp();
        return;
      }
      if (modifier && event.shiftKey && key === "m") {
        event.preventDefault();
        showPanel("problems");
        return;
      }
      if (modifier && event.shiftKey && key === "u") {
        event.preventDefault();
        showPanel("output");
        return;
      }
      if (modifier && event.shiftKey && key === "e") {
        event.preventDefault();
        showActivity("explorer");
        return;
      }
      if (modifier && event.shiftKey && key === "f") {
        event.preventDefault();
        showActivity("search");
        return;
      }
      if (modifier && event.shiftKey && key === "g") {
        event.preventDefault();
        showActivity("scm");
        return;
      }
      if (event.key === "F12" && event.shiftKey) {
        event.preventDefault();
        event.stopPropagation();
        void findReferences();
      }
    };
    window.addEventListener("keydown", onKeyDown, true);
    return () => window.removeEventListener("keydown", onKeyDown, true);
  }, []);
}
