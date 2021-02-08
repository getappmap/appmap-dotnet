#!/bin/bash

BINPATH="$( cd "$(dirname "$0")" && cd ../out && pwd )"

export CORECLR_ENABLE_PROFILING=1
export CORECLR_PROFILER={324F817A-7420-4E6D-B3C1-143FBED6D855}
export CORECLR_PROFILER_PATH_64=$BINPATH/libInstrumentationEngine.so
export CORECLR_PROFILER_PATH=$BINPATH/libInstrumentationEngine.so
export MicrosoftInstrumentationEngine_LogLevel=Error
export MicrosoftInstrumentationEngine_DisableCodeSignatureValidation=1
export MicrosoftInstrumentationEngine_FileLogPath=/dev/stderr
export MicrosoftInstrumentationEngine_ConfigPath64_TestMethod=$BINPATH/ProductionBreakpoints_x64.config

exec "$@"
