using System;
using Xunit;

namespace AppMap.Test
{
    namespace Code {
        public class Statics {
            public static string? NullableString(bool giveValue) {
                if (giveValue)
                    return "test";
                else
                    return null;
            }

            public static Guid? NullableGuid(bool giveValue) {
                if (giveValue)
                    return Guid.Empty;
                else
                    return null;
            }
        }
    }

    public class StaticsTest
    {
        [Fact]
        public void NullableString()
        {
            Console.WriteLine(Code.Statics.NullableString(true));
            Console.WriteLine(Code.Statics.NullableString(false));
        }

        [Fact]
        public void NullableGuid()
        {
            Console.WriteLine(Code.Statics.NullableGuid(true));
            Console.WriteLine(Code.Statics.NullableGuid(false));
        }
    }
}
