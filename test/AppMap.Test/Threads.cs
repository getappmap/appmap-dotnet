using System;
using System.Threading;
using Xunit;

namespace AppMap.Test
{
    namespace Code {
        public class Threads {
            static public void Fun1() { Thread.Sleep(100); }
            static public void Fun2() { Thread.Sleep(100); }
        }
    }

    public class ThreadsTest {
        [Fact]
        public void CanInterleave()
        {
            var t1 = new Thread(new ThreadStart(Code.Threads.Fun1));
            var t2 = new Thread(new ThreadStart(Code.Threads.Fun2));

            t1.Start();
            Thread.Sleep(10);
            t2.Start();

            t1.Join();
            t2.Join();
        }
    }
}
