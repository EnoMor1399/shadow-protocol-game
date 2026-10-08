import { createHmac } from 'node:crypto';

type Counter = {
  count: number;
  resetAt: number;
};

export class FixedWindowLimiter {
  private readonly entries = new Map<string, Counter>();

  constructor(
    private readonly maxAttempts: number,
    private readonly windowMs: number,
    private readonly maxEntries = 10_000
  ) {
    if (maxAttempts < 1 || windowMs < 1 || maxEntries < 100) throw new Error('Invalid limiter configuration');
  }

  consume(key: string, now = Date.now()): boolean {
    this.prune(now);
    const current = this.entries.get(key);
    if (!current || current.resetAt <= now) {
      if (this.entries.size >= this.maxEntries) {
        const oldest = this.entries.keys().next().value as string | undefined;
        if (oldest) this.entries.delete(oldest);
      }
      this.entries.set(key, { count: 1, resetAt: now + this.windowMs });
      return true;
    }
    if (current.count >= this.maxAttempts) return false;
    current.count += 1;
    return true;
  }

  private prune(now: number): void {
    for (const [key, value] of this.entries) {
      if (value.resetAt <= now) this.entries.delete(key);
    }
  }
}

export function credentialFingerprint(provider: string, authToken: string, secret: string): string {
  return createHmac('sha256', secret).update(provider).update('\0').update(authToken).digest('hex');
}
