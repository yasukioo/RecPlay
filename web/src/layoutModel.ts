export function getAppShellClasses(): {
  app: string;
  root: string;
  main: string;
  leftRail: string;
  center: string;
  rightRail: string;
} {
  return {
    app: "min-h-screen bg-hmi-bg text-hmi-text",
    root: "flex min-h-screen flex-col",
    main: "flex-1 overflow-y-auto xl:grid xl:grid-cols-[220px_minmax(0,1fr)_280px] xl:overflow-hidden",
    leftRail: "border-b border-hmi-border p-2 xl:overflow-y-auto xl:border-b-0 xl:border-r",
    center: "space-y-3 overflow-visible p-3 sm:p-4 xl:overflow-y-auto",
    rightRail: "border-t border-hmi-border p-3 sm:p-4 xl:overflow-y-auto xl:border-t-0 xl:border-l",
  };
}

export function getPanelGridClasses(): string {
  return "grid grid-cols-1 gap-3 lg:grid-cols-2";
}

export function getSessionInfoClasses(): string {
  return "space-y-4";
}

export function getStatusBarClasses(): {
  footer: string;
  left: string;
  right: string;
} {
  return {
    footer: "flex flex-wrap items-center justify-between gap-x-4 gap-y-1 border-t border-hmi-border bg-hmi-surface px-3 py-2 text-xs sm:px-4",
    left: "flex flex-wrap items-center gap-x-4 gap-y-1",
    right: "flex w-full flex-wrap items-center gap-x-4 gap-y-1 text-hmi-text-muted md:w-auto md:justify-end",
  };
}

export function getMonitorDashboardClasses(): {
  metricGrid: string;
  chartGrid: string;
  trendGrid: string;
} {
  return {
    metricGrid: "grid grid-cols-1 gap-2 sm:grid-cols-2 xl:grid-cols-3",
    chartGrid: "grid grid-cols-1 gap-3 xl:grid-cols-[minmax(0,2fr)_minmax(220px,1fr)_minmax(220px,1fr)]",
    trendGrid: "grid grid-cols-1 gap-3 sm:grid-cols-2 xl:grid-cols-4",
  };
}

export function getMapperClasses(): {
  channelGrid: string;
  mappingRow: string;
} {
  return {
    channelGrid: "grid grid-cols-1 gap-4 text-xs md:grid-cols-[minmax(0,1fr)_120px_minmax(0,1fr)]",
    mappingRow: "grid grid-cols-1 gap-2 rounded bg-hmi-surface-alt px-2 py-2 lg:grid-cols-[minmax(0,1fr)_minmax(0,1fr)_auto]",
  };
}

export function getPluginCardClasses(): {
  configGrid: string;
  actions: string;
} {
  return {
    configGrid: "grid grid-cols-1 gap-3 md:grid-cols-2",
    actions: "flex flex-wrap gap-2 border-t border-hmi-border pt-3",
  };
}
