# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- This changelog, finally.

## [0.0.4] - 2021-08-01

### Added
- Function parameter names are now captured.
- MacOS support.

### Fixed
- Long test names are truncated in appmap filenames to prevent exceeding
  maximum file name length.

## [0.0.3] - 2021-07-20

### Fixed
- Launcher now works correctly even if the host system is missing *libdl.so*
  symlink to *libdl.so.2*.

## [0.0.2] - 2021-07-20

### Added
- Collect return values.
- Integration tests.
- An option to `exclude` classes from instrumentation in the config file.
- Capture ASP.NET core HTTP requests and responses.
- Sanity checks when using launcher, to allow catching library load issues early.
- Configuration for packaging as a dotnet tool.

### Changes
- VSTest integration no longer uses a stub data collector; instead it hooks
  into VSTest directly. This simplifies the setup significantly.
- Implement dotnet-appmap script as a portable .net executable launcher instead.

### Fixed
- More robust return capture in the presence of branches.
- Signal failure when opening output files.

## [0.0.1] - 2021-03-25

### Added
- Initial release.
- Record function calls and returns.
- Option to dump a simple classmap.
- VSTest data collector to hook test cases.
- Convenience run and scripts.
- Configuration file and variables support.


[Unreleased]: https://github.com/applandinc/appmap-dotnet/compare/v0.0.4...HEAD
[0.0.4]: https://github.com/applandinc/appmap-dotnet/compare/v0.0.3...v0.0.4
[0.0.3]: https://github.com/applandinc/appmap-dotnet/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/applandinc/appmap-dotnet/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/applandinc/appmap-dotnet/releases/tag/v0.0.1
