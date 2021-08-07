using System;
using System.Threading;
using Xunit;

namespace AppMap.Test
{
    namespace Code {
        public class Threads {
            public static SemaphoreSlim sem1;
            public static SemaphoreSlim sem2;
            static public void Fun1() {
                sem1.Wait();
            }
            static public void Fun2() {
                sem1.Release();
                sem2.Wait();
            }
        }
    }

    public class ThreadsTest {
        [Fact]
        public void CanInterleave()
        {
            Code.Threads.sem1 = new SemaphoreSlim(0);
            Code.Threads.sem2 = new SemaphoreSlim(0);

            var t1 = new Thread(new ThreadStart(Code.Threads.Fun1));
            var t2 = new Thread(new ThreadStart(Code.Threads.Fun2));

            t1.Start();
            t2.Start();

            t1.Join();
            Code.Threads.sem2.Release();
            t2.Join();
        }
    }
}
