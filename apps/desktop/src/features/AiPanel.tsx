import { SparklesIcon } from "../components/Icons";

export function AiPanel() {
  return (
    <div className="sidebar-panel">
      <div className="sidebar-header">Anvil AI</div>
      <div className="ai-panel">
        <div className="ai-empty">
          <SparklesIcon size={28} />
          <h3>AI assistance is opt-in</h3>
          <p>
            Anvil 0.0.1 ships without an AI provider. The assistant panel is scheduled for
            Phase 4 with explicit consent, secret redaction, and local-model support.
          </p>
          <p className="muted">
            No source code leaves this machine from this panel. There is no telemetry.
          </p>
        </div>
        <div className="ai-input disabled">
          <textarea disabled placeholder="AI assistance is disabled in this release" />
          <div className="ai-disabled-note">Phase 4</div>
        </div>
      </div>
    </div>
  );
}
