using System;
using Xunit;

namespace AppMap.Test
{
    namespace Code {
        public class DisablableConsole {
            public static bool Disabled = true;

            // This method in _release_ mode compiles to CIL that
            // breaks CLRIE's SingleReturnDefaultInstrumentation.
            // I've pretty much minimized it, please don't touch it.
            public static void Write(object? value)
            {
                if (value is null || Disabled)
                {
                    return;
                }

                Console.WriteLine(value.ToString());
            }
        }
    }

    public class FlowTest {
        [Fact]
        public void ExcerciseDisablableConsole()
        {
            Code.DisablableConsole.Write(null);
            Code.DisablableConsole.Write(new object());
            Code.DisablableConsole.Write("something else");
        }
    }
}
