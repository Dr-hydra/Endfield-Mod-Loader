using EModLoader.Core;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;

if (args.Length != 1) throw new ArgumentException("Supply the product native Release directory.");
string productNative = Path.GetFullPath(args[0]);
string root = Path.Combine(Path.GetTempPath(), "eml-launch-tests-" + Guid.NewGuid());
Directory.CreateDirectory(root);
string native = Path.Combine(root, "controlled runtime");
Directory.CreateDirectory(native);
foreach (string file in new[] { "EModLoader.Host.dll", "EModLoader.XInputProxy.dll", "EMLControlledTarget.exe", "EMLForeignXInput.dll" })
    File.Copy(Path.Combine(productNative, file), Path.Combine(native, file));
// This separately compiled fixture accepts controlled unsigned parents. The
// shipped Launcher retains mandatory official discovery and signature checks.
File.Copy(Path.Combine(productNative, "EMLControlledLauncher.exe"), Path.Combine(native, "EModLoader.Launcher.exe"));
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
    Check(!settings.LoadAllowXInputConflict(), "Missing settings enabled coexistence by default.");
    await settings.SaveAsync(a);
    Check(new GamePathSettings(Path.Combine(root, "settings")).Load() == a, "Selected path did not persist across instances.");
    Check(!settings.LoadAllowXInputConflict(), "Legacy path-only settings enabled coexistence.");
    await settings.SaveAllowXInputConflictAsync(true);
    Check(new GamePathSettings(Path.Combine(root, "settings")).LoadAllowXInputConflict() && settings.Load() == a, "Saving coexistence lost the selected game path or opt-in.");
    await settings.SaveAsync(b);
    Check(settings.Load() == b && settings.LoadAllowXInputConflict(), "Changing game path lost the opt-in.");
    await settings.SaveAllowXInputConflictAsync(false);
    Check(!new GamePathSettings(Path.Combine(root, "settings")).LoadAllowXInputConflict() && settings.Load() == b, "Disabling coexistence did not persist.");
    await settings.SaveAsync(a);
    await Reject<InvalidDataException>(() => settings.SaveAsync(fakeLauncher), "Invalid selection was saved.");
    Check(settings.Load() == a, "Rejected selection damaged saved settings.");
    File.Delete(a);
    Check(GamePathDiscovery.Discover(settings.Load(), [b], [root]).SequenceEqual(new[] { b }), "Stale saved path blocked rediscovery.");
    File.WriteAllText(settings.FilePath, "[]"); Check(settings.Load() == "", "Array settings were not handled.");
    File.WriteAllText(settings.FilePath, "{broken"); Check(settings.Load() == "", "Corrupt settings were not handled.");
    Check(!settings.LoadAllowXInputConflict(), "Corrupt settings enabled coexistence.");
    File.WriteAllText(settings.FilePath, "{\"allow_xinput_conflict\":\"true\",\"extension\":42}");
    Check(!settings.LoadAllowXInputConflict(), "A string setting was mistaken for an enabled boolean.");
    await Task.WhenAll(settings.SaveAsync(b), settings.SaveAllowXInputConflictAsync(true));
    Check(settings.Load() == b && settings.LoadAllowXInputConflict(), "Concurrent path/option updates lost data.");
    Check(System.Text.Json.Nodes.JsonNode.Parse(File.ReadAllText(settings.FilePath))!["extension"]!.GetValue<int>() == 42, "Saving launcher preferences lost unrelated fields.");
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
    int compatiblePid = await GameLauncher.LaunchAsync(store, fakeGame, native, null,
        _ => { runs++; return Task.FromResult(Success()); }, allowXInputConflict: true);
    Check(compatiblePid == 123 && runs == 1, "Explicit coexistence opt-in did not reach launch.");
    ProductPaths.UseRuntimeDirectory(native);
    await Reject<InvalidOperationException>(() => GameLauncher.DeployProxyAsync(store, fakeGame, true), "Foreign XInput install protection was lost.");
    await Reject<InvalidOperationException>(() => GameLauncher.DeployProxyAsync(store, fakeGame, false), "Foreign XInput uninstall protection was lost.");
    Check(File.ReadAllBytes(Path.Combine(Path.GetDirectoryName(fakeGame)!, "xinput1_4.dll")).SequenceEqual(new byte[] { 0 }), "Foreign DLL changed during deployment rejection.");
    File.Delete(Path.Combine(Path.GetDirectoryName(fakeGame)!, "xinput1_4.dll"));
    await Reject<InvalidOperationException>(() => GameLauncher.LaunchAsync(store, fakeGame, native, officialParent: ""), "Real launcher accepted a bad executable.");

    // Actual C# -> native named-pipe result, Host loading and authentication.
    // Only a project-owned controlled executable runs; never a game or UAC UI.
    string eventName = @"Local\EML.LaunchTests.Stop." + Guid.NewGuid().ToString("N");
    using var stop = new EventWaitHandle(false, EventResetMode.ManualReset, eventName);
    string? previous = Environment.GetEnvironmentVariable("EML_TEST_STOP");
    Environment.SetEnvironmentVariable("EML_TEST_STOP", eventName);
    try
    {
        pid = await GameLauncher.LaunchAsync(store, Path.Combine(native, "EMLControlledTarget.exe"), native, officialParent: "");
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
        {
            stop.Reset();
            File.Copy(Path.Combine(native, "EMLControlledTarget.exe"), fakeGame, true);
            string foreignPath = Path.Combine(Path.GetDirectoryName(fakeGame)!, "xinput1_4.dll");
            File.Copy(Path.Combine(native, "EMLForeignXInput.dll"), foreignPath);
            byte[] originalDll = File.ReadAllBytes(foreignPath);
            Environment.SetEnvironmentVariable("EML_TEST_LOAD_FOREIGN_XINPUT", "1");
            try
            {
                await Reject<InvalidOperationException>(() => GameLauncher.LaunchAsync(store, fakeGame, native), "Launcher was permissive without opt-in.");
                pid = await GameLauncher.LaunchAsync(store, fakeGame, native, allowXInputConflict: true, officialParent: "");
                using var compatibleProcess = Process.GetProcessById(pid);
                _ = compatibleProcess.Handle; // Retain the exit-code handle before the fixture exits.
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
        stop.Reset();
        string parentName = "EMLParent_" + Guid.NewGuid().ToString("N");
        string parentPath = Path.Combine(root, parentName + ".exe");
        File.Copy(Path.Combine(native, "EMLControlledTarget.exe"), parentPath);
        await Reject<InvalidOperationException>(() => GameLauncher.LaunchAsync(store,
            Path.Combine(native, "EMLControlledTarget.exe"), productNative, officialParent: parentPath),
            "Production launcher accepted a controlled parent with the wrong name.");
        string unsignedOfficial = Path.Combine(root, "Games.exe");
        File.Copy(parentPath, unsignedOfficial);
        await Reject<InvalidOperationException>(() => GameLauncher.LaunchAsync(store,
            Path.Combine(native, "EMLControlledTarget.exe"), productNative, officialParent: unsignedOfficial),
            "Production launcher accepted an unsigned Games.exe.");
        Check(Process.GetProcessesByName(parentName).Length == 0, "Rejected parent validation started a process.");
        var timer = Stopwatch.StartNew();
        pid = await GameLauncher.LaunchAsync(store, Path.Combine(native, "EMLControlledTarget.exe"), native, officialParent: parentPath);
        using var bridgedGame = Process.GetProcessById(pid);
        Check(timer.Elapsed < TimeSpan.FromSeconds(15) && !bridgedGame.HasExited, "Bridge blocked IPC until game exit.");
        var parents = Process.GetProcessesByName(parentName);
        Check(parents.Length == 1, "Bridge did not create exactly one owned parent.");
        using var ownedParent = parents.Single();
        Check(BridgeProcessInfo.ParentId(bridgedGame) == ownedParent.Id, "Game did not inherit the selected official-parent handle.");
        Check(!ownedParent.HasExited, "Owned parent ended before its game.");
        stop.Set();
        await bridgedGame.WaitForExitAsync().WaitAsync(TimeSpan.FromSeconds(10));
        await ownedParent.WaitForExitAsync().WaitAsync(TimeSpan.FromSeconds(10));
        Check(ownedParent.HasExited, "Owned suspended parent leaked after game exit.");

        // A bad Host forces failure after both child processes were created.
        // Neither the unfinished game nor its suspended parent may survive.
        stop.Reset();
        string failingRuntime = Path.Combine(root, "failed host runtime");
        Directory.CreateDirectory(failingRuntime);
        File.Copy(Path.Combine(native, "EModLoader.Launcher.exe"), Path.Combine(failingRuntime, "EModLoader.Launcher.exe"));
        File.WriteAllBytes(Path.Combine(failingRuntime, "EModLoader.Host.dll"), [0]);
        string failureTargetName = "EMLFailure_" + Guid.NewGuid().ToString("N");
        string failureTarget = Path.Combine(root, failureTargetName + ".exe");
        File.Copy(Path.Combine(native, "EMLControlledTarget.exe"), failureTarget);
        await Reject<InvalidOperationException>(() => GameLauncher.LaunchAsync(store, failureTarget,
            failingRuntime, officialParent: parentPath), "Invalid Host unexpectedly loaded.");
        Check(Process.GetProcessesByName(failureTargetName).Length == 0, "Failed injection left its game running.");
        Check(Process.GetProcessesByName(parentName).Length == 0, "Failed injection leaked its suspended parent.");

        // Another game's version must not trigger a managed-side special case
        // or require a hand-written launcher.json before the helper is called.
        string metadata = Path.Combine(Path.GetDirectoryName(fakeGame)!, "Endfield_Data");
        Directory.CreateDirectory(metadata);
        foreach (string version in new[] { "1.6.3", "1.7.0" })
        {
            File.WriteAllText(Path.Combine(metadata, "globalgamemanagers"), version);
            int versionPid = await GameLauncher.LaunchAsync(store, fakeGame, native, run: _ => Task.FromResult(Success()),
                allowXInputConflict: true);
            Check(versionPid == 123, "A game version required manual parent configuration.");
        }
    }
    finally { stop.Set(); Environment.SetEnvironmentVariable("EML_TEST_STOP", previous); }
    Console.WriteLine($"PASS: {checks} saved-preference, game-path, elevation-policy, XInput protection, native result and controlled Host checks. Interactive UAC and actual games were not tested.");
}
finally
{
    string relative = Path.GetRelativePath(Path.GetTempPath(), root);
    if (relative.StartsWith("eml-launch-tests-", StringComparison.Ordinal) && !relative.Contains(Path.DirectorySeparatorChar))
        Directory.Delete(root, true);
}

internal static class BridgeProcessInfo
{
    [StructLayout(LayoutKind.Sequential)]
    private struct Basic { public nint Reserved1, Peb, Reserved2, Reserved3, Pid, Parent; }
    [DllImport("ntdll.dll")]
    private static extern int NtQueryInformationProcess(nint process, int infoClass, out Basic value, int length, out int used);
    public static int ParentId(Process process)
    {
        if (NtQueryInformationProcess(process.Handle, 0, out var value, Marshal.SizeOf<Basic>(), out _) != 0)
            throw new InvalidOperationException("Cannot inspect controlled process parent.");
        return (int)value.Parent;
    }
}
