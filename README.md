# AppMap-dotnet

AppMap-dotnet records the execution of .net code. Note currently only Linux is supported.

## Usage

An `dotnet-appmap` script is provided for ease of use; it automatically configures the runtime environment.
See details below (or the script itself), or just dive in with the release tarball:
```sh-session
$ tar xvf ~/Download/appmap-dotnet-*.tar.bz2 -C ~/opt
$ ln -s ~/opt/appmap-dotnet-*/dotnet-appmap ~/.local/bin # or anywhere on the PATH
$ cd ~/projects/myproject
$ echo "packages: [ class: MyProject ]" > appmap.yml
$ APPMAP_OUTPUT_PATH=/dev/stdout dotnet appmap exec bin/myproject.dll
$ dotnet appmap test
```

Note it currently requires libxml2 and libunwind8, you might need to install these two. If libraries are missing it will silently fail to work.

### Details

You need [CLR Instrumentation Engine](https://github.com/microsoft/CLRInstrumentationEngine/)
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

For convenience [run](scripts/appmap-dotnet) script is provided that sets it all up for the `out` subdirectory, as built
by `docker-build.sh`. If you're building locally, you can `ln -sf build/libappmap-instrumentation.so out`
so that the current binary is used.

### Microsoft Testing Framework (aka VSTest) integration

Directory `vstest` contains a data collector which, when used together with the instrumentation, enables
generating individual appmaps for each test case; this works with any test framework integrated with MSTest.
Runsettings file to set up the collector correctly is included and automatically used when running `dotnet appmap test`.

## Configuration

### Configuration file

Appmap-dotnet instruments only specific code packages; which are these can be configured by
creating an `appmap.yml` file in the project root directory, for example:

```yaml
name: my-project
packages:
- class: MyProject.UtilityClass
- module: MyProject.Business.dll
- path: /usr/lib/util
```

The path to the file can be explicitly set in `APPMAP_CONFIG` environment variable. Otherwise, appmap-dotnet
searches current directory (or `APPMAP_BASEPATH` if set) and all its ancestors for `appmap.yml`.
Relative `path` entries are resolved in `APPMAP_BASEPATH` or the directory where `appmap.yml` was found.

### Environment variables

#### `APPMAP_BASEPATH`

Base path; this is where the search for the config file begins and where relative `path` packages are resolved.
Defaults to where the config file was found, or the current directory.

#### `APPMAP_CLASSMAP`

If set and truthy, generate a classmap in the appmap files.
Currently disabled by default because the vscode extension chokes on classmaps without source location information.

#### `APPMAP_CONFIG`

File path. Allows using a specific config file. By default, `appmap.yml` is searched in the current
directory and its ancestors.

#### `APPMAP_LIST_MODULES`

File path. If set, the list of all module names seen is printed there on shutdown.
On Linux, you can use `/dev/stdout` or `/dev/stderr` to dump it to console.

#### `APPMAP_OUTPUT_DIR`

When instrumenting unit tests, appmaps are written to this directory. Defaults to
`$APPMAP_BASEPATH/tmp/appmap`.

#### `APPMAP_OUTPUT_PATH`

File path. If set, an appmap encompassing the whole execution is saved there on shutdown.

#### `APPMAP_LOG_LEVEL`

Log level, one of `trace`, `debug`, `info`, `warning`, `error`, `critical`, `off`.
Defaults to `info`.

## Building

The repository is pretty self-contained and should build on any Linux with modern cmake and C++ compiler.
`vstest` directory requires dotnet sdk to build.

You can use `scripts/docker-build.sh` to build both CLRIE and this instrumentation method in Docker.
On success, `out` directory will contain all the files needed to use this instrumentation method.
