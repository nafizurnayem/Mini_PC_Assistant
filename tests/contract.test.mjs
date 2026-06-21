import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const contract = JSON.parse(await readFile(new URL('../contracts/mqtt-contract.json', import.meta.url), 'utf8'));

test('all topics are unique and relative', () => {
  const topics = Object.values(contract.topics);
  assert.equal(new Set(topics).size, topics.length);
  for (const topic of topics) {
    assert.ok(!topic.startsWith('/'));
    assert.ok(!topic.includes('#'));
    assert.ok(!topic.includes('+'));
  }
});

test('dangerous off action remains explicit', () => {
  assert.ok(contract.command.actions.includes('off'));
  assert.equal(contract.defaults.commandTtlSeconds, 30);
});

test('alert defaults have recovery hysteresis', () => {
  const t = contract.defaults.thresholds;
  assert.ok(t.hysteresis > 0);
  assert.ok(t.cpuTempC > t.hysteresis);
  assert.ok(t.gpuTempC > t.hysteresis);
});
