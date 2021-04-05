using System;

namespace hello
{
    class Program
    {
        static int Test(string? text) {
            if (text == null) {
                Console.WriteLine("null test");
                return 0;
            }
            return 1;
        }

        static void Test2() {
            Console.WriteLine("test2");
        }

        static void Main(string[] args)
        {
            Console.WriteLine(Test("text"));
            Test2();
            Console.WriteLine("Hello World!");
        }
    }
}
