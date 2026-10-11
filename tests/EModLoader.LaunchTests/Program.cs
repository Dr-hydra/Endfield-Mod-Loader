using EModLoader.Core;
using System.ComponentModel;
using System.Diagnostics;

if (args.Length != 1) throw new ArgumentException("Supply the product native Release directory.");
string native = Path.GetFullPath(args[0]);
string root = Path.Combine(Path.GetTempPath(), "eml-launch-tests-" + Guid.NewGuid());
Directory.CreateDirectory(root);
int checks = 0;
void Check(bool value, string message) { if (!value) throw new Exception(message); checks++; }
async Task Reject<T>(Func<Task> action, string message) where T : Exception
{ try { await action(); } catch (T) { checks++; return; } throw new Exception(message); }
string Install(string relative)
{
    string folder = Path.Combine(root, relative); Directory.CreateDirectory(folder);
    string executable = Path.GetFullPath(Path.Combine(folder, "Endfield.exe")); File.WriteAllBytes(executable, [0]); return executable;
}
LaunchReport Success(int pid = 123) => new("eml.launch.v1", 0, 0, pid, "");
LaunchReport Failure(int code) => new("eml.launch.v1", 4, code, 0, "test failure");
try
{
    string a = Install("Games/Endfield 中文 & space's/Endfield Game"), b = Install("Program Files/Endfield Global");
    string deep = Install("unrelated/subfolder/Endfield Game");
    string fakeLauncher = Path.Combine(root, "EndfieldLauncher.exe"); File.WriteAllBytes(fakeLauncher, [0]);
    Check(GamePathDiscovery.Resolve(fakeLauncher) is null, "A launcher was mistaken for the game.");
    Check(GamePathDiscovery.Resolve('"' + a + '"') == a, "Quoted/unicode path did not resolve.");
    Check(GamePathDiscovery.Resolve(Path.GetDirectoryName(a)!) == a, "Installation directory did not resolve.");
    var paths = GamePathDiscovery.Discover(a, [Path.GetDirectoryName(b)!, a], [root]);
    Check(paths.SequenceEqual(new[] { a, b }), "Preferred/registry/common directory discovery lost ordering or deduplication.");
    Check(!paths.Contains(deep), "Bounded discovery crawled unrelated folders.");
    var settings = new GamePathSettings(Path.Combine(root, "settings"));
    await settings.SaveAsync(a);
    Check(new GamePathSettings(Path.Combine(root, "settings")).Load() == a, "Selected path did not persist across instances.");
    await Reject<InvalidDataException>(() => settings.SaveAsync(fakeLauncher), "Invalid selection was saved.");
    Check(settings.Load() == a, "Rejected selection damaged saved settings.");
    File.Delete(a);
    Check(GamePathDiscovery.Discover(settings.Load(), [b], [root]).SequenceEqual(new[] { b }), "Stale saved path blocked rediscovery.");
    File.WriteAllText(settings.FilePath, "[]"); Check(settings.Load() == "", "Array settings were not handled.");
    File.WriteAllText(settings.FilePath, "{broken"); Check(settings.Load() == "", "Corrupt settings were not handled.");
    Check(!Directory.EnumerateFiles(Path.GetDirectoryName(settings.FilePath)!, "*.pending").Any(), "Settings left a temporary file.");

    var attempts = new List<bool>(); string progress = "";
    int pid = await GameLauncher.LaunchWithElevationAsync(elevated => { attempts.Add(elevated); return Task.FromResult(elevated ? Success() : Failure(740)); }, text => progress = text);
    Check(pid == 123 && attempts.SequenceEqual(new[] { false, true }) && progress.Length > 0, "740 did not cause exactly one elevated retry.");
    attempts.Clear();
    pid = await GameLauncher.LaunchWithElevationAsync(elevated => { attempts.Add(elevated); return Task.FromResult(Success()); }, null);
    Check(pid == 123 && attempts.SequenceEqual(new[] { false }), "Normal success unnecessarily elevated.");
    attempts.Clear();
    await Reject<InvalidOperationException>(() => GameLauncher.LaunchWithElevationAsync(elevated => { attempts.Add(elevated); return Task.FromResult(Failure(5)); }, null), "Native failure was accepted.");
    Check(attempts.SequenceEqual(new[] { false }), "Non-740 failures caused elevation.");
    await Reject<OperationCanceledException>(() => GameLauncher.LaunchWithElevationAsync(elevated => elevated
        ? Task.FromException<LaunchReport>(new Win32Exception(1223)) : Task.FromResult(Failure(740)), null), "UAC cancellation did not stop launch.");
    attempts.Clear();
    await Reject<InvalidOperationException>(() => GameLauncher.LaunchWithElevationAsync(elevated => { attempts.Add(elevated); return Task.FromResult(Failure(740)); }, null), "Repeated 740 did not fail.");
    Check(attempts.Count == 2, "Elevation kept retrying.");
    await Reject<InvalidDataException>(() => GameLauncher.LaunchWithElevationAsync(_ => Task.FromResult(new LaunchReport("wrong", 4, 740, 0, "")), null), "Invalid IPC protocol was accepted.");
    await Reject<InvalidOperationException>(() => GameLauncher.LaunchWithElevationAsync(_ => Task.FromResult(Success(0)), null), "Invalid process ID was accepted.");
    pid = await GameLauncher.LaunchWithElevationAsync(elevated => elevated ? Task.FromResult(Success())
        : Task.FromException<LaunchReport>(new Win32Exception(740)), null);
    Check(pid == 123, "A helper-level elevation requirement was not handled.");
    attempts.Clear();
    var deployed = await GameLauncher.RunWithElevationAsync(elevated => {
        attempts.Add(elevated); return Task.FromResult(elevated ? Success(0) : Failure(5));
    }, null, true);
    Check(deployed.ExitCode == 0 && deployed.ProcessId == 0 && attempts.SequenceEqual(new[] { false, true }), "Deployment permissions did not elevate exactly once.");
    await Reject<OperationCanceledException>(() => GameLauncher.RunWithElevationAsync(elevated => elevated
        ? Task.FromException<LaunchReport>(new Win32Exception(1223)) : Task.FromResult(Failure(5)), null, true), "Deployment UAC cancellation was lost.");

    var store = new ModuleStore(Path.Combine(root, "module library with 空格")); await store.EnsureAsync();
    int runs = 0;
    string fakeGame = Install("isolated invalid executable");
    File.WriteAllBytes(Path.Combine(Path.GetDirectoryName(fakeGame)!, "xinput1_4.dll"), [0]);
    await Reject<InvalidOperationException>(() => GameLauncher.LaunchAsync(store, fakeGame, native, null,
        _ => { runs++; return Task.FromResult(Success()); }), "Proxy conflict guard was bypassed.");
    Check(runs == 0, "Conflict was checked after launch.");
    if (BuildFlavor.IsCompatibilityTest)
    {
        int compatiblePid = await GameLauncher.LaunchAsync(store, fakeGame, native, null,
            _ => { runs++; return Task.FromResult(Success()); }, allowXInputConflict: true);
        Check(compatiblePid == 123 && runs == 1, "Explicit compatibility opt-in did not reach launch.");
    }
    else
    {
        await Reject<InvalidOperationException>(() => GameLauncher.LaunchAsync(store, fakeGame, native, null,
            _ => { runs++; return Task.FromResult(Success()); }, allowXInputConflict: true), "Normal build accepted compatibility opt-in.");
        Check(runs == 0, "Normal build ran an opted-in conflicting launch.");
    }
    ProductPaths.UseRuntimeDirectory(native);
    await Reject<InvalidOperationException>(() => GameLauncher.DeployProxyAsync(store, fakeGame, true), "Foreign XInput install protection was lost.");
    await Reject<InvalidOperationException>(() => GameLauncher.DeployProxyAsync(store, fakeGame, false), "Foreign XInput uninstall protection was lost.");
    Check(File.ReadAllBytes(Path.Combine(Path.GetDirectoryName(fakeGame)!, "xinput1_4.dll")).SequenceEqual(new byte[] { 0 }), "Foreign DLL changed during deployment rejection.");
    File.Delete(Path.Combine(Path.GetDirectoryName(fakeGame)!, "xinput1_4.dll"));
    await Reject<InvalidOperationException>(() => GameLauncher.LaunchAsync(store, fakeGame, native), "Real launcher accepted a bad executable.");

    // Actual C# -> native named-pipe result, Host loading and authentication.
    // Only a project-owned controlled executable runs; never a game or UAC UI.
    string eventName = @"Local\EML.LaunchTests.Stop." + Guid.NewGuid().ToString("N");
    using var stop = new EventWaitHandle(false, EventResetMode.ManualReset, eventName);
    string? previous = Environment.GetEnvironmentVariable("EML_TEST_STOP");
    Environment.SetEnvironmentVariable("EML_TEST_STOP", eventName);
    try
    {
        pid = await GameLauncher.LaunchAsync(store, Path.Combine(native, "EMLControlledTarget.exe"), native);
        using var process = Process.GetProcessById(pid); Check(!process.HasExited, "IPC reported an exited target.");
        await using var host = new HostConnection(store);
        bool connected = false;
        for (int i = 0; i < 50; i++)
        {
            try { var status = await host.RequestAsync("status"); connected = status["mode"]?.GetValue<string>() == "game"; }
            catch (HttpRequestException) { }
            if (connected) break; await Task.Delay(100);
        }
        Check(connected, "Controlled target Host did not authenticate after named-pipe launch.");
        stop.Set(); await process.WaitForExitAsync().WaitAsync(TimeSpan.FromSeconds(10));
        if (BuildFlavor.IsCompatibilityTest)
        {
            stop.Reset();
            File.Copy(Path.Combine(native, "EMLControlledTarget.exe"), fakeGame, true);
            string foreignPath = Path.Combine(Path.GetDirectoryName(fakeGame)!, "xinput1_4.dll");
            File.Copy(Path.Combine(native, "EMLForeignXInput.dll"), foreignPath);
            byte[] originalDll = File.ReadAllBytes(foreignPath);
            Environment.SetEnvironmentVariable("EML_TEST_LOAD_FOREIGN_XINPUT", "1");
            try
            {
                await Reject<InvalidOperationException>(() => GameLauncher.LaunchAsync(store, fakeGame, native), "Compatibility build was permissive without opt-in.");
                pid = await GameLauncher.LaunchAsync(store, fakeGame, native, allowXInputConflict: true);
                using var compatibleProcess = Process.GetProcessById(pid);
                connected = false;
                for (int i = 0; i < 50; i++)
                {
                    try
                    {
                        var status = await host.RequestAsync("status");
                        connected = status["mode"]?.GetValue<string>() == "game" &&
                            compatibleProcess.Modules.Cast<ProcessModule>().Any(module => module.ModuleName.Equals("xinput1_4.dll", StringComparison.OrdinalIgnoreCase));
                    }
                    catch (HttpRequestException) { }
                    if (connected) break; await Task.Delay(100);
                }
                Check(connected, "Foreign XInput and EML Host did not coexist in the controlled target.");
                Check(File.ReadAllBytes(foreignPath).SequenceEqual(originalDll), "Compatibility launch changed the foreign DLL.");
                stop.Set(); await compatibleProcess.WaitForExitAsync().WaitAsync(TimeSpan.FromSeconds(10));
                Check(compatibleProcess.ExitCode == 0, "Foreign XInput fixture did not load correctly.");
            }
            finally { Environment.SetEnvironmentVariable("EML_TEST_LOAD_FOREIGN_XINPUT", null); }
        }
    }
    finally { stop.Set(); Environment.SetEnvironmentVariable("EML_TEST_STOP", previous); }
    Console.WriteLine($"PASS: {checks} game-path, elevation-policy, XInput protection, native result and controlled Host checks. Compatibility build: {BuildFlavor.IsCompatibilityTest}. Interactive UAC and actual games were not tested.");
}
finally
{
    string relative = Path.GetRelativePath(Path.GetTempPath(), root);
    if (relative.StartsWith("eml-launch-tests-", StringComparison.Ordinal) && !relative.Contains(Path.DirectorySeparatorChar))
        Directory.Delete(root, true);
}
