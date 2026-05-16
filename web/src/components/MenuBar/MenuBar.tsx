export function MenuBar() {
  // TODO: Codex — implement File/Edit/View/Help menu items with dropdown behavior
  return (
    <header className="flex items-center justify-between px-4 py-2 bg-hmi-surface border-b border-hmi-border">
      <div className="flex items-center gap-4">
        <span className="text-hmi-accent font-bold text-lg">RecPlay</span>
        <nav className="flex gap-3 text-sm text-hmi-text-muted">
          {/* TODO: Codex — dropdown menus */}
          <button type="button" className="hover:text-hmi-text">File</button>
          <button type="button" className="hover:text-hmi-text">View</button>
          <button type="button" className="hover:text-hmi-text">Plugins</button>
          <button type="button" className="hover:text-hmi-text">Help</button>
        </nav>
      </div>
      <div className="flex items-center gap-2 text-xs text-hmi-text-muted">
        {/* TODO: Codex — connection indicator + settings gear icon */}
        <span>v0.2.0</span>
      </div>
    </header>
  );
}
