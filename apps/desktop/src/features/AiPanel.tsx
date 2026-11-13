import { SparklesIcon } from "../components/Icons";
import { useI18n } from "../i18n";

export function AiPanel() {
  const { t } = useI18n();
  return (
    <div className="sidebar-panel">
      <div className="sidebar-header">{t("ai.title")}</div>
      <div className="ai-panel">
        <div className="ai-empty">
          <SparklesIcon size={28} />
          <h3>{t("ai.optInTitle")}</h3>
          <p>{t("ai.optInBody")}</p>
          <p className="muted">{t("ai.noLeakNote")}</p>
        </div>
        <div className="ai-input disabled">
          <textarea disabled placeholder={t("ai.disabledPlaceholder")} />
          <div className="ai-disabled-note">{t("common.phase4")}</div>
        </div>
      </div>
    </div>
  );
}
