using PcBot.Service;

var builder = Host.CreateApplicationBuilder(args);
builder.Services.AddWindowsService(options => options.ServiceName = "PC Assistant Bot");
builder.Services.AddSingleton<HardwareMonitor>();
builder.Services.AddSingleton<AlertEvaluator>();
builder.Services.AddSingleton<SpeedTestRunner>();
builder.Services.AddHostedService<AgentWorker>();
await builder.Build().RunAsync();

