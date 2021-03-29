using System;

namespace hello
{
    class Program
    {
        static void Test(string? text) {
            if (text == null) {
                Console.WriteLine("null test");
            }
        }

        static void Test2() {
            Console.WriteLine("test2");
        }

        static void Main(string[] args)
        {
            Test("text");
            Test2();
            Console.WriteLine("Hello World!");
        }
    }
}
