import { CheckIcon, CloseIcon, ErrorIcon, InfoIcon, WarningIcon } from "../components/Icons";
import { useUiStore } from "../stores/ui";
import { useI18n } from "../i18n";

export function Toasts() {
  const { t } = useI18n();
  const toasts = useUiStore((state) => state.toasts);
  const dismiss = useUiStore((state) => state.dismissToast);
  if (toasts.length === 0) return null;
  return (
    <div className="toast-stack" aria-live="polite" aria-atomic="false">
      {toasts.map((toast) => (
        <div className={`toast ${toast.kind}`} key={toast.id} role={toast.kind === "error" ? "alert" : "status"}>
          <span className="toast-icon">
            {toast.kind === "error" ? (
              <ErrorIcon size={15} />
            ) : toast.kind === "warning" ? (
              <WarningIcon size={15} />
            ) : toast.kind === "success" ? (
              <CheckIcon size={15} />
            ) : (
              <InfoIcon size={15} />
            )}
          </span>
          <span className="toast-message">{toast.message}</span>
          <button
            type="button"
            className="toast-close"
            aria-label={t("toast.dismiss")}
            onClick={() => dismiss(toast.id)}
          >
            <CloseIcon size={11} />
          </button>
        </div>
      ))}
    </div>
  );
}
