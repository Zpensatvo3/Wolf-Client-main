// MSBuild's CL task expects a compiler executable, while sccache expects the
// real compiler as its first argument. Keep argument boundaries intact here.
using System;
using System.Diagnostics;
using System.Text;

internal static class CompilerCache
{
    private static string Quote(string argument)
    {
        var result = new StringBuilder("\"");
        int slashes = 0;
        foreach (char character in argument)
        {
            if (character == '\\') { slashes++; continue; }
            if (character == '"')
            {
                result.Append('\\', slashes * 2 + 1).Append('"');
                slashes = 0;
                continue;
            }
            result.Append('\\', slashes).Append(character);
            slashes = 0;
        }
        return result.Append('\\', slashes * 2).Append('"').ToString();
    }

    private static int Main(string[] arguments)
    {
        string cache = Environment.GetEnvironmentVariable("SCCACHE_PATH");
        string compiler = Environment.GetEnvironmentVariable("ONESTEP_REAL_CL");
        if (String.IsNullOrEmpty(cache) || String.IsNullOrEmpty(compiler))
        {
            Console.Error.WriteLine("Compiler cache requires SCCACHE_PATH and ONESTEP_REAL_CL.");
            return 1;
        }
        var command = new StringBuilder(Quote(compiler));
        foreach (string argument in arguments) command.Append(' ').Append(Quote(argument));
        try
        {
            using (var process = Process.Start(new ProcessStartInfo(cache, command.ToString())
                   { UseShellExecute = false }))
            {
                process.WaitForExit();
                return process.ExitCode;
            }
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine("Compiler cache launcher: " + exception.Message);
            return 1;
        }
    }
}
