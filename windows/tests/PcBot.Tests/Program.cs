using PcBot.Core;
using PcBot.Service;

var now = DateTimeOffset.UtcNow;
var current = new CommandEnvelope("one", "mobile", "on", now, now.AddSeconds(30));
Assert(!current.IsExpired(now), "current command should be accepted");
Assert((current with { ExpiresAt = now }).IsExpired(now), "expired command should be rejected");
Assert((current with { IssuedAt = now.AddMinutes(2) }).IsExpired(now), "future command should be rejected");

var topics = new MqttTopics("desktop-01");
Assert(topics.PowerCommand == "pcbot/v1/desktop-01/command/power", "power topic mismatch");
Assert(topics.Ack == "pcbot/v1/desktop-01/event/ack", "ack topic mismatch");

var options = new AgentOptions { AlertSustainSeconds = 15, AlertCooldownSeconds = 300 };
var evaluator = new AlertEvaluator();
var hot = Snapshot(now, cpu: 95);
Assert(evaluator.Evaluate(hot, options).Count == 0, "alert should wait for sustain period");
var raised = evaluator.Evaluate(Snapshot(now.AddSeconds(16), cpu: 95), options);
Assert(raised.Count == 1 && raised[0].Active, "sustained alert should be raised");
Assert(evaluator.Evaluate(Snapshot(now.AddSeconds(20), cpu: 94), options).Count == 0,
    "active alert should respect cooldown");
var recovered = evaluator.Evaluate(Snapshot(now.AddSeconds(21), cpu: 80), options);
Assert(recovered.Count == 1 && !recovered[0].Active, "recovery should be published after hysteresis");

Console.WriteLine("PcBot Windows tests passed.");

static TelemetrySnapshot Snapshot(DateTimeOffset timestamp, double cpu) => new(
    timestamp,
    new Metric(cpu, 30),
    new Metric(60, 20),
    new MemoryMetric(40, 4, 10),
    Array.Empty<StorageMetric>(),
    new NetworkMetric(0, 0, "test"),
    100);

static void Assert(bool condition, string message)
{
    if (!condition) throw new InvalidOperationException(message);
}
