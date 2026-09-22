import { config } from "./config.js";

const levels = ["fatal", "error", "warn", "info", "debug", "trace"] as const;
type Level = (typeof levels)[number];

const threshold = levels.indexOf(config.LOG_LEVEL);

function emit(level: Level, a: unknown, b?: string): void {
  if (levels.indexOf(level) > threshold) return;
  const [context, message] = typeof a === "string" ? [undefined, a] : [a, b ?? ""];
  const line = `${new Date().toISOString()} [${level.toUpperCase()}] ${message}`;
  const out = level === "error" || level === "fatal" ? console.error : console.log;
  // Never log secrets: values come from device messages, not from config.
  if (context) out(line, JSON.stringify(context));
  else out(line);
}

export const log = {
  fatal: (a: unknown, b?: string) => emit("fatal", a, b),
  error: (a: unknown, b?: string) => emit("error", a, b),
  warn: (a: unknown, b?: string) => emit("warn", a, b),
  info: (a: unknown, b?: string) => emit("info", a, b),
  debug: (a: unknown, b?: string) => emit("debug", a, b),
  trace: (a: unknown, b?: string) => emit("trace", a, b),
};
