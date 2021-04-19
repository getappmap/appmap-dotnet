using System;
using Xunit;

namespace AppMap.Test
{
    namespace Code {
        public class Values {
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

            public Uri? Uri { get; set; }

            public static Span<byte> Span() {
                return new Span<byte>(new byte[4]);
            }

            public static T? Generic<T>(T? v) {
                return v;
            }
        }
    }

    public class ValuesTest
    {
        [Fact]
        public void NullableString()
        {
            Console.WriteLine(Code.Values.NullableString(true));
            Console.WriteLine(Code.Values.NullableString(false));
        }

        [Fact]
        public void NullableGuid()
        {
            Console.WriteLine(Code.Values.NullableGuid(true));
            Console.WriteLine(Code.Values.NullableGuid(false));
        }

        [Fact]
        public void NullableUri()
        {
            var v = new Code.Values();
            Console.WriteLine(v.Uri);
            v.Uri = new Uri("http://appmap.test");
            Console.WriteLine(v.Uri);
        }

        [Fact]
        public void Span()
        {
            Console.WriteLine(Code.Values.Span().ToString());
        }

        [Fact]
        public void Generic()
        {
            Console.WriteLine(Code.Values.Generic("testing"));
            Console.WriteLine(Code.Values.Generic<string>(null));
            Console.WriteLine(Code.Values.Generic(Guid.Empty));
        }
    }
}
