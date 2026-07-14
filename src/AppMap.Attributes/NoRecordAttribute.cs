using System;

namespace AppMap;

/// <summary>
/// Opts a test (or a whole test class / assembly) out of automatic per-test
/// AppMap recording. Entirely optional: tests record unmodified when the
/// agent is attached; apply this only where recording is unwanted.
/// Matched by full type name, so referencing this dependency-free package is
/// the only requirement.
/// </summary>
[AttributeUsage(AttributeTargets.Method | AttributeTargets.Class | AttributeTargets.Assembly)]
public sealed class NoRecordAttribute : Attribute
{
}
