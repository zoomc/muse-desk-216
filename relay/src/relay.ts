/**
 * muse-desk-216 notification relay (runs on Mac mini).
 *
 * Subscribes to Muse chat via `muse-client` (`MuseClient.subscribe()` opens
 * the `/chat/subscribe` long-lived stream) and forwards each finished
 * `message.assistant` message to stdout or to a local webhook.
 *
 * What this deliberately does NOT do:
 * - no history backfill: messages sent while disconnected are lost forever;
 * - no sending: the relay never calls `sendMessage()`, so it produces no
 *   echo of its own (the seen-set below only filters duplicate deliveries
 *   of the same `message_id`, e.g. `delta.message_done` + `message.assistant`
 *   for one reply, or redelivery guards across streams).
 *
 * Reconnect policy (implemented here; muse-client has none): exponential
 * backoff starting at 1s, doubling, capped at 60s, plus jitter.
 */
import { MuseClient } from 'muse-client';
import { loadCredentials, saveCredentials } from 'muse-client/credentials';

const BACKOFF_INITIAL_MS = 1000;
const BACKOFF_MAX_MS = 60000;
const SEEN_CAP = 1000;

interface RelayConfig {
  credentialsDir: string | undefined;
  sessionId: string | undefined;
  vmId: string | undefined;
  webhookUrl: string | undefined;
}

function readConfig(): RelayConfig {
  return {
    // Undefined = muse-client default (macOS: ~/Library/Application Support/MuseGadgetPair).
    credentialsDir: process.env.RELAY_CREDENTIALS_DIR || undefined,
    sessionId: process.env.RELAY_SESSION_ID || undefined,
    vmId: process.env.RELAY_VM_ID || undefined,
    // Unset = print to stdout. Set = POST JSON to this URL instead.
    webhookUrl: process.env.RELAY_WEBHOOK_URL || undefined,
  };
}

/** Equal-jitter backoff: attempt 0 -> ~1s, doubling, capped at 60s. */
export function backoffDelayMs(attempt: number): number {
  const exp = Math.min(BACKOFF_MAX_MS, BACKOFF_INITIAL_MS * 2 ** attempt);
  return exp / 2 + Math.random() * (exp / 2);
}

function messageIdOf(payload: Record<string, unknown>, raw: Record<string, unknown>): string {
  const id = payload.message_id ?? raw.message_id ?? payload.id;
  return typeof id === 'string' ? id : '';
}

function messageTextOf(payload: Record<string, unknown>): string {
  const text = payload.display_text ?? payload.content;
  return typeof text === 'string' ? text : '';
}

async function deliver(config: RelayConfig, messageId: string, text: string): Promise<void> {
  if (!config.webhookUrl) {
    process.stdout.write(`Muse: ${text}\n`);
    return;
  }
  try {
    const response = await fetch(config.webhookUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ message_id: messageId, text }),
      signal: AbortSignal.timeout(10000),
    });
    if (!response.ok) {
      console.error(`relay: webhook ${response.status} for ${messageId}`);
    }
  } catch (error) {
    console.error(`relay: webhook failed for ${messageId}: ${error instanceof Error ? error.message : error}`);
  }
}

async function runOnce(
  config: RelayConfig,
  seen: Set<string>,
  signal: AbortSignal,
): Promise<void> {
  const credentials = await loadCredentials(config.credentialsDir);
  const client = await MuseClient.connect({
    credentials,
    vmId: config.vmId,
    onCredentials: (next) => saveCredentials(next, config.credentialsDir),
  });
  try {
    const events = await client.subscribe({ signal, sessionId: config.sessionId });
    try {
      for await (const event of events) {
        if (event.event !== 'message.assistant') continue;
        // A `display_text_ready === false` marker carries no finished text yet.
        if (event.payload.display_text_ready === false) continue;
        const id = messageIdOf(event.payload, event.raw);
        if (!id || seen.has(id)) continue;
        seen.add(id);
        if (seen.size > SEEN_CAP) {
          const oldest = seen.values().next().value;
          if (oldest !== undefined) seen.delete(oldest);
        }
        await deliver(config, id, messageTextOf(event.payload));
      }
      // The server closed the stream without an abort: treat as a failure so
      // the outer loop reconnects with backoff instead of spinning.
      throw new Error('Muse subscription ended; reconnecting');
    } finally {
      events.close();
    }
  } finally {
    client.close();
  }
}

async function main(): Promise<void> {
  const config = readConfig();
  const controller = new AbortController();
  const stop = (): void => controller.abort();
  process.on('SIGINT', stop);
  process.on('SIGTERM', stop);

  const seen = new Set<string>();
  let attempt = 0;
  while (!controller.signal.aborted) {
    try {
      await runOnce(config, seen, controller.signal);
      attempt = 0;
    } catch (error) {
      if (controller.signal.aborted) break;
      console.error(`relay: ${error instanceof Error ? error.message : error}`);
      const delay = backoffDelayMs(attempt);
      attempt += 1;
      await new Promise<void>((resolve) => {
        const timer = setTimeout(resolve, delay);
        controller.signal.addEventListener('abort', () => {
          clearTimeout(timer);
          resolve();
        }, { once: true });
      });
    }
  }
}

await main();
