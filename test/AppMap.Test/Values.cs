using System;
using Xunit;

namespace AppMap.Test
{
    namespace Code {
        public struct AStruct {
            public int field;
        }

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

            public static void ByRef(ref Uri? uri, ref bool i) {
                if (uri is null)
                    Console.WriteLine("null");
                else
                    Console.WriteLine(uri.ToString());

                uri = new Uri("http://appmap.test");
                i = false;
            }

            public static void TakesStruct(AStruct s) {
                Console.WriteLine(s);
            }

            public static void StructRef(ref AStruct s) {
                Console.WriteLine(s);
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

        [Fact]
        public void ByRef()
        {
            Uri? uri = null;
            bool i = true;
            Code.Values.ByRef(ref uri, ref i);
            Code.Values.ByRef(ref uri, ref i);
        }

        [Fact]
        public void Struct()
        {
            var s = new Code.AStruct{field = 5};
            Code.Values.TakesStruct(s);
            Code.Values.StructRef(ref s);
        }
    }
}
