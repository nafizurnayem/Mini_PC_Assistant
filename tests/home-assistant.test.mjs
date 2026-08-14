import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

const packageYaml = await readFile(
  new URL('../home-assistant/config/packages/pcbot.yaml', import.meta.url),
  'utf8',
);
const dashboard = await readFile(
  new URL('../home-assistant/config/ui-lovelace.yaml', import.meta.url),
  'utf8',
);

test('Home Assistant commands use expiring, non-retained QoS 1 envelopes', () => {
  assert.match(packageYaml, /expires_at:/);
  assert.match(packageYaml, /qos:\s*1/g);
  assert.match(packageYaml, /retain:\s*false/g);
  assert.match(packageYaml, /action":"wifi"/);
  assert.match(packageYaml, /action":"factory"/);
});

test('Home Assistant exposes telemetry, speed test, and protected reset controls', () => {
  for (const feature of [
    'CPU Temperature',
    'GPU Temperature',
    'Memory',
    'Speed Test Download',
    'Latest Alert',
  ]) {
    assert.match(packageYaml, new RegExp(feature));
  }
  assert.match(dashboard, /confirmation:/);
  assert.match(dashboard, /history-graph/);
});
