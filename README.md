# AppMap-dotnet

## Usage

Currently only Linux is supported. You need [CLR Instrumentation Engine](https://github.com/microsoft/CLRInstrumentationEngine/)
binary, [config/ProductionBreakpoints_x64.config](config/ProductionBreakpoints_x64.config) and the built binary from this project.
Put all three files, `libInstrumentationEngine.so`, `ProductionBreakpoints_x64.config` and `libappmap-instrumentation.so` in the same
directory. Set the environment variables (replace `opt/appmap-dotnet` with the path to the files):
```
CORECLR_PROFILER={324F817A-7420-4E6D-B3C1-143FBED6D855}
CORECLR_PROFILER_PATH_64=opt/appmap-dotnet/libInstrumentationEngine.so
CORECLR_PROFILER_PATH=opt/appmap-dotnet/libInstrumentationEngine.so
MicrosoftInstrumentationEngine_LogLevel=Error
CORECLR_ENABLE_PROFILING=1
MicrosoftInstrumentationEngine_DisableCodeSignatureValidation=1
MicrosoftInstrumentationEngine_FileLogPath=/dev/stderr
MicrosoftInstrumentationEngine_ConfigPath64_TestMethod=opt/appmap-dotnet/ProductionBreakpoints_x64.config
```

For convenience [run](scripts/run.sh) script is provided that sets it all up for the `out` subdirectory, as built
by `docker-build.sh`. If you're building locally, you can `ln -sf build/libappmap-instrumentation.so out`
so that the current binary is used.

## Configuration

### Environment variables

#### `APPMAP`

If set, methods are instrumented and an appmap is produced.

#### `APPMAP_METHOD_LIST`

If set, the list of all methods ever seen is printed on shutdown. Note it usually doesn't make sense to use it together with `APPMAP`.

#### `APPMAP_OUTPUT_PATH`

Path to the output file. **Note it will be appended to**, which is especially useful with `APPMAP_METHOD_LIST` and if several processes are run (like usually happens when running `dotnet test`, for example).
If not set, the output is printed to the standard output stream.

## Building

The repository is pretty self-contained and should build on any Linux with modern cmake and C++ compiler.

You can use `scripts/docker-build.sh` to build both CLRIE and this instrumentation method in Docker.
On success, `out` directory will contain all the files needed to use this instrumentation method.
