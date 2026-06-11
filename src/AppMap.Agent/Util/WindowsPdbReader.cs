using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace AppMap.Util;

/// <summary>
/// Best-effort sequence-point reader for classic (Windows) PDBs via the
/// native diasymreader binder — the fallback for .NET Framework builds that
/// cannot use &lt;DebugType&gt;portable&lt;/DebugType&gt;. Windows-only; on any
/// failure (binder not registered, mismatched PDB, non-Windows OS) callers
/// fall back to "no source locations", which is the agent's behavior for a
/// missing PDB.
/// </summary>
internal static class WindowsPdbReader
{
    public static bool IsSupported =>
        RuntimeInformation.IsOSPlatform(OSPlatform.Windows);

    /// <summary>Opens a reader for the assembly, or null.</summary>
    public static ISymUnmanagedReader? Open(Assembly assembly)
    {
        if (!IsSupported)
            return null;
        try
        {
            var location = assembly.Location;
            if (string.IsNullOrEmpty(location))
                return null;

            // IMetaDataDispenser -> IMetaDataImport for the assembly, which
            // the binder uses to pair methods with PDB entries.
            var dispenser = (IMetaDataDispenser)Activator.CreateInstance(
                Type.GetTypeFromCLSID(Clsid.CorMetaDataDispenser, throwOnError: true)!)!;
            var importIid = Iid.IMetaDataImport;
            dispenser.OpenScope(location, 0 /* read */, ref importIid, out var import);

            var binder = (ISymUnmanagedBinder)Activator.CreateInstance(
                Type.GetTypeFromCLSID(Clsid.CorSymBinderSxS, throwOnError: true)!)!;
            var hr = binder.GetReaderForFile(import, location, null, out var reader);
            return hr == 0 ? reader : null;
        }
        catch (Exception e)
        {
            Logger.Debug($"Windows PDB unavailable for {assembly.GetName().Name}: {e.Message}");
            return null;
        }
    }

    /// <summary>First non-hidden sequence point of the method, or nulls.</summary>
    public static (string? Path, int? LineNo) Locate(ISymUnmanagedReader reader, MethodBase method)
    {
        try
        {
            if (reader.GetMethod(method.MetadataToken, out var symMethod) != 0)
                return (null, null);
            symMethod.GetSequencePointCount(out var count);
            if (count <= 0)
                return (null, null);

            var offsets = new int[count];
            var documents = new ISymUnmanagedDocument[count];
            var lines = new int[count];
            var columns = new int[count];
            var endLines = new int[count];
            var endColumns = new int[count];
            symMethod.GetSequencePoints(count, out var actual, offsets, documents,
                lines, columns, endLines, endColumns);

            for (var i = 0; i < actual; i++)
            {
                // 0xFEEFEE marks a hidden sequence point.
                if (lines[i] is 0 or 0xFEEFEE || documents[i] == null)
                    continue;
                return (UrlOf(documents[i]), lines[i]);
            }
        }
        catch (Exception e)
        {
            Logger.Debug($"no Windows PDB info for {method.Name}: {e.Message}");
        }
        return (null, null);
    }

    private static string? UrlOf(ISymUnmanagedDocument document)
    {
        document.GetUrl(0, out var length, null);
        if (length <= 0)
            return null;
        var buffer = new StringBuilder(length);
        document.GetUrl(length, out _, buffer);
        return buffer.ToString();
    }

    private static class Clsid
    {
        public static readonly Guid CorMetaDataDispenser =
            new("E5CB7A31-7512-11D2-89CE-0080C792E5D8");

        public static readonly Guid CorSymBinderSxS =
            new("0A29FF9E-7F9C-4437-8B11-F424491E3931");
    }

    private static class Iid
    {
        public static readonly Guid IMetaDataImport =
            new("7DAC8207-D3AE-4C75-9B67-92801A497D44");
    }

    // Minimal COM declarations. Unused vtable slots are declared as
    // placeholders so the slot order matches corsym.idl exactly.

    [ComImport, Guid("809C652E-7396-11D2-9771-00A0C9B4D50C"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMetaDataDispenser
    {
        void DefineScope_Placeholder();

        void OpenScope([MarshalAs(UnmanagedType.LPWStr)] string szScope, int dwOpenFlags,
            ref Guid riid, [MarshalAs(UnmanagedType.IUnknown)] out object punk);
    }

    [ComImport, Guid("AA544D42-28CB-11D3-BD22-0000F80849BD"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface ISymUnmanagedBinder
    {
        [PreserveSig]
        int GetReaderForFile([MarshalAs(UnmanagedType.IUnknown)] object importer,
            [MarshalAs(UnmanagedType.LPWStr)] string fileName,
            [MarshalAs(UnmanagedType.LPWStr)] string? searchPath,
            out ISymUnmanagedReader reader);
    }

    [ComImport, Guid("B4CE6286-2A6B-3712-A3B7-1EE1DAD467B5"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface ISymUnmanagedReader
    {
        void GetDocument_Placeholder();
        void GetDocuments_Placeholder();
        void GetUserEntryPoint_Placeholder();

        [PreserveSig]
        int GetMethod(int token, out ISymUnmanagedMethod method);
    }

    [ComImport, Guid("B62B923C-B500-3158-A543-24F307A8B7E1"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface ISymUnmanagedMethod
    {
        void GetToken_Placeholder();

        void GetSequencePointCount(out int count);

        void GetRootScope_Placeholder();
        void GetScopeFromOffset_Placeholder();
        void GetOffset_Placeholder();
        void GetRanges_Placeholder();
        void GetParameters_Placeholder();
        void GetNamespace_Placeholder();
        void GetSourceStartEnd_Placeholder();

        void GetSequencePoints(int cPoints, out int pcPoints,
            [In, Out, MarshalAs(UnmanagedType.LPArray)] int[] offsets,
            [In, Out, MarshalAs(UnmanagedType.LPArray)] ISymUnmanagedDocument[] documents,
            [In, Out, MarshalAs(UnmanagedType.LPArray)] int[] lines,
            [In, Out, MarshalAs(UnmanagedType.LPArray)] int[] columns,
            [In, Out, MarshalAs(UnmanagedType.LPArray)] int[] endLines,
            [In, Out, MarshalAs(UnmanagedType.LPArray)] int[] endColumns);
    }

    [ComImport, Guid("40DE4037-7C81-3E1E-B022-AE1ABFF2CA08"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface ISymUnmanagedDocument
    {
        void GetUrl(int cchUrl, out int pcchUrl,
            [MarshalAs(UnmanagedType.LPWStr)] StringBuilder? szUrl);
    }
}
