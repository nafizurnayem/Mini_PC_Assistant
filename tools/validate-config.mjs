import { readFile, readdir } from 'node:fs/promises';
import { resolve } from 'node:path';

const root = resolve(import.meta.dirname, '..');
const contract = JSON.parse(await readFile(resolve(root, 'contracts/mqtt-contract.json'), 'utf8'));
const requiredTopics = ['powerCommand', 'speedTestCommand', 'resetCommand', 'pcState', 'botState', 'speedTestState', 'telemetry', 'alert', 'ack'];

for (const topic of requiredTopics) {
  if (!contract.topics[topic]) throw new Error(`Missing topic: ${topic}`);
}

const requiredFiles = [
  'firmware/platformio.ini',
  'windows/PcBot.sln',
  'mobile/pubspec.yaml',
  'home-assistant/compose.yaml',
];

for (const file of requiredFiles) {
  await readFile(resolve(root, file));
}

console.log(`Contract v${contract.version}: ${Object.keys(contract.topics).length} topics valid`);

