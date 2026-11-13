import { Component, type ErrorInfo, type ReactNode } from "react";

interface Props {
  children: ReactNode;
}

interface State {
  error: Error | null;
}

export class ErrorBoundary extends Component<Props, State> {
  state: State = { error: null };

  static getDerivedStateFromError(error: Error): State {
    return { error };
  }

  componentDidCatch(error: Error, info: ErrorInfo): void {
    // The shell forwards core logs; this is the last-resort UI crash record.
    console.error("Anvil UI crashed", error, info.componentStack);
  }

  render(): ReactNode {
    if (this.state.error) {
      return (
        <div className="crash-screen">
          <div className="welcome-logo">
            Anvil<span>IDE</span>
          </div>
          <h1>The editor UI crashed</h1>
          <p>{this.state.error.message}</p>
          <button type="button" className="primary-button" onClick={() => window.location.reload()}>
            Reload UI
          </button>
        </div>
      );
    }
    return this.props.children;
  }
}
