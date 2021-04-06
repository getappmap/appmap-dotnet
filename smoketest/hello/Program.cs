using System;

namespace hello
{
    class Program
    {
        static string Test(string? text) {
            if (text == null) {
                Console.WriteLine("null test");
                return "Foo";
            }
            return "Bar";
        }

        static void Test2() {
            Console.WriteLine("test2");
            Console.WriteLine("test2".ToString());
        }

        static void Main(string[] args)
        {
            Console.WriteLine(Test("text"));
            Test2();
            Console.WriteLine("Hello World!");
        }
    }
}
