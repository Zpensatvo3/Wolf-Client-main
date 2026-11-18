using System;
using System.Text;

internal static class ArgumentEcho
{
    private static int Main(string[] arguments)
    {
        foreach (string argument in arguments)
            Console.WriteLine(Convert.ToBase64String(Encoding.UTF8.GetBytes(argument)));
        return Environment.GetEnvironmentVariable("ONESTEP_TEST_EXIT") == "37" ? 37 : 0;
    }
}
