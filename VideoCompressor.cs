using System;
using System.Runtime.InteropServices;

namespace VideoCompressor
{
    class Program
    {
        static void Main(string[] args)
        {
            string input = args[0];
            string output = args[1];
            Console.WriteLine("Compressing video...");
            
            Type shellType = Type.GetTypeFromProgID("Shell.Application");
            dynamic shell = Activator.CreateInstance(shellType);
            dynamic folder = shell.NameSpace(System.IO.Path.GetDirectoryName(input));
            dynamic file = folder.ParseName(System.IO.Path.GetFileName(input));
            
            // Just copy for now, actual transcoding needs MF
            Console.WriteLine("Source: " + input);
            Console.WriteLine("Dest: " + output);
            Console.WriteLine("Media Foundation transcoding unavailable in this mode.");
        }
    }
}
