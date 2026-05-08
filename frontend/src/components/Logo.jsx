// CHAOS brand mark: three offset jagged shards around a central void,
// inscribed roughly in a circle. Uses currentColor so it inherits theme accent.
export default function Logo() {
  return (
    <div className="logo">
      <svg
        className="logo-mark"
        viewBox="0 0 32 32"
        width="22"
        height="22"
        aria-hidden="true"
      >
        {/* faint outer ring */}
        <circle
          cx="16"
          cy="16"
          r="14"
          fill="none"
          stroke="currentColor"
          strokeOpacity="0.25"
          strokeWidth="1"
        />
        {/* three shards radiating from center */}
        <path
          d="M16 3 L19 12 L16 15 L13 12 Z"
          fill="currentColor"
        />
        <path
          d="M28 20 L18 18 L17 14 L21 13 Z"
          fill="currentColor"
          opacity="0.85"
        />
        <path
          d="M7 26 L13 17 L17 17 L15 22 Z"
          fill="currentColor"
          opacity="0.7"
        />
        {/* center spark */}
        <circle cx="16" cy="16" r="1.6" fill="currentColor" />
      </svg>
      <span className="logo-text">CHAOS</span>
    </div>
  );
}
