import type { ReactNode, SVGProps } from "react";

interface IconProps extends SVGProps<SVGSVGElement> {
  size?: number;
}

function Base({ size = 16, children, ...props }: IconProps & { children: ReactNode }) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.4"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
      focusable="false"
      {...props}
    >
      {children}
    </svg>
  );
}

export const FilesIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M3 2.5h5l2 2v9H3z" />
    <path d="M8 2.5v2h2" />
    <path d="M5.2 7h3.6M5.2 9.5h3.6" />
  </Base>
);

export const SearchIcon = (props: IconProps) => (
  <Base {...props}>
    <circle cx="7" cy="7" r="4" />
    <path d="m10 10 3 3" />
  </Base>
);

export const BranchIcon = (props: IconProps) => (
  <Base {...props}>
    <circle cx="4.5" cy="3.5" r="1.5" />
    <circle cx="4.5" cy="12.5" r="1.5" />
    <circle cx="11.5" cy="5.5" r="1.5" />
    <path d="M4.5 5v6M6 5.5h3.5a2 2 0 0 1 2 2v3" />
  </Base>
);

export const SparklesIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M7 2.5 8 5l2.5 1L8 7 7 9.5 6 7 3.5 6 6 5z" />
    <path d="M12 9.5 12.7 11l1.5.7-1.5.8-.7 1.5-.8-1.5-1.5-.8 1.5-.7z" />
  </Base>
);

export const SettingsIcon = (props: IconProps) => (
  <Base {...props}>
    <circle cx="8" cy="8" r="2.2" />
    <path d="M8 1.8v1.4M8 12.8v1.4M1.8 8h1.4M12.8 8h1.4M3.6 3.6l1 1M11.4 11.4l1 1M12.4 3.6l-1 1M4.6 11.4l-1 1" />
  </Base>
);

export const ShieldIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M8 1.8 13 3.6v4.2c0 3-2 5.3-5 6.4-3-1.1-5-3.4-5-6.4V3.6z" />
    <path d="m5.8 8 1.5 1.5 3-3" />
  </Base>
);

export const ChevronRightIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="m6 3.5 4.5 4.5L6 12.5" />
  </Base>
);

export const ChevronDownIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="m3.5 6 4.5 4.5L12.5 6" />
  </Base>
);

export const FolderIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M1.8 4.2h4l1.2 1.4h7.2v6.6a1 1 0 0 1-1 1H2.8a1 1 0 0 1-1-1z" />
    <path d="M1.8 6h12.4" />
  </Base>
);

export const FolderOpenIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M1.8 4.2h4l1.2 1.4h6.2a1 1 0 0 1 1 1v1.2H1.8z" />
    <path d="M3 7.8h12l-1.2 5.4H1.8z" />
  </Base>
);

export const FileIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M3.5 1.8h5l3 3v9.4h-8z" />
    <path d="M8.5 1.8v3h3" />
  </Base>
);

export const CloseIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="m4 4 8 8M12 4l-8 8" />
  </Base>
);

export const PlusIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M8 3v10M3 8h10" />
  </Base>
);

export const NewFileIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M3.5 1.8h5l3 3v3.4" />
    <path d="M8.5 1.8v3h3M11 9v5M8.5 11.5h5" />
  </Base>
);

export const NewFolderIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M1.8 4.2h4l1.2 1.4h6.2a1 1 0 0 1 1 1v2.2" />
    <path d="M1.8 6v6.6a1 1 0 0 0 1 1h5.4M11 9.5v5M8.5 12h5" />
  </Base>
);

export const RefreshIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M13 8a5 5 0 1 1-1.5-3.6" />
    <path d="M13 2.5V5h-2.5" />
  </Base>
);

export const CollapseIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M2.5 3.5h5M2.5 8h5M2.5 12.5h5M10.5 5l2.5 3-2.5 3" />
  </Base>
);

export const TrashIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M3 4.5h10M6 4.5V3h4v1.5M4.2 4.5l.7 9.3h6.2l.7-9.3" />
    <path d="M6.5 7v4.5M9.5 7v4.5" />
  </Base>
);

export const EditIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M3 13h3l7-7-3-3-7 7z" />
    <path d="m9 4 3 3" />
  </Base>
);

export const PlayIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M4.5 2.8 13 8l-8.5 5.2z" />
  </Base>
);

export const BugIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M5 5.5h6v4a3 3 0 0 1-6 0z" />
    <path d="M5 7H2.5M11 7h2.5M5 10H2.8M11 10h2.2M6 3.5 5 2M10 3.5l1-1.5M6.5 5V3.5M9.5 5V3.5" />
  </Base>
);

export const TerminalIcon = (props: IconProps) => (
  <Base {...props}>
    <rect x="1.8" y="2.8" width="12.4" height="10.4" rx="1" />
    <path d="m4.5 6 2 2-2 2M8.5 10.5h3" />
  </Base>
);

export const ProblemsIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M8 1.8 14.2 13H1.8z" />
    <path d="M8 6v3.2M8 11.2v.2" />
  </Base>
);

export const OutputIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M2.5 3.5h11M2.5 6.5h8M2.5 9.5h10M2.5 12.5h6" />
  </Base>
);

export const CheckIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="m3 8.5 3 3 7-7" />
  </Base>
);

export const InfoIcon = (props: IconProps) => (
  <Base {...props}>
    <circle cx="8" cy="8" r="6" />
    <path d="M8 7.2v4M8 4.8v.2" />
  </Base>
);

export const WarningIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M8 1.8 14.2 13H1.8z" />
    <path d="M8 6v3.2M8 11.2v.2" />
  </Base>
);

export const ErrorIcon = (props: IconProps) => (
  <Base {...props}>
    <circle cx="8" cy="8" r="6" />
    <path d="m5.8 5.8 4.4 4.4M10.2 5.8 5.8 10.2" />
  </Base>
);

export const LockIcon = (props: IconProps) => (
  <Base {...props}>
    <rect x="3.2" y="7" width="9.6" height="6.3" rx="1" />
    <path d="M5 7V5a3 3 0 0 1 6 0v2" />
  </Base>
);

export const ArrowLeftIcon = (props: IconProps) => (
  <Base {...props}>
    <path d="M13 8H3M6.5 4.5 3 8l3.5 3.5" />
  </Base>
);
