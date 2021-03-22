using Microsoft.VisualStudio.TestPlatform.ObjectModel.DataCollection;
using Microsoft.VisualStudio.TestPlatform.ObjectModel.DataCollector.InProcDataCollector;
using Microsoft.VisualStudio.TestPlatform.ObjectModel.InProcDataCollector;

namespace AppMap
{
    public class DataCollector : InProcDataCollection
    {
        public void Initialize(IDataCollectionSink dataCollectionSink) {
        }

        public void TestSessionStart(TestSessionStartArgs testSessionStartArgs) {
        }

        public void TestCaseStart(TestCaseStartArgs testCaseStartArgs) {
            StartCase(testCaseStartArgs.TestCase.DisplayName);
        }

        public void TestCaseEnd(TestCaseEndArgs testCaseEndArgs) {
            EndCase();
        }

        public void TestSessionEnd(TestSessionEndArgs testSessionEndArgs) {
        }

        // instrumentation hooks
        private static void StartCase(string name) {}
        private static void EndCase() {}
    }
}
