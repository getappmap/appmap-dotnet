using System;
using System.Threading;
﻿using Microsoft.VisualStudio.TestPlatform.ObjectModel.DataCollection;
using Microsoft.VisualStudio.TestPlatform.ObjectModel.DataCollector.InProcDataCollector;
using Microsoft.VisualStudio.TestPlatform.ObjectModel.InProcDataCollector;

namespace AppMap
{
    public class DataCollector : InProcDataCollection
    {
        static void Main(string[] args) {
            Type t = typeof(DataCollector);
            Console.WriteLine($"Assembly Qualified Name: {t.AssemblyQualifiedName}");
        }

        public void Initialize(IDataCollectionSink dataCollectionSink) {
            Console.WriteLine($"data collector initialize {Thread.CurrentThread.ManagedThreadId}");
        }

        public void TestSessionStart(TestSessionStartArgs testSessionStartArgs) {
            Console.WriteLine($"data collector session start {Thread.CurrentThread.ManagedThreadId}");
        }

        public void TestCaseStart(TestCaseStartArgs testCaseStartArgs) {
            Console.WriteLine($"data collector test case start {Thread.CurrentThread.ManagedThreadId}");
        }

        public void TestCaseEnd(TestCaseEndArgs testCaseEndArgs) {
            Console.WriteLine($"data collector test case end {Thread.CurrentThread.ManagedThreadId}");
        }

        public void TestSessionEnd(TestSessionEndArgs testSessionEndArgs) {
            Console.WriteLine($"data collector session end {Thread.CurrentThread.ManagedThreadId}");
        }
    }
}
