Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class MFTools
{
    const uint MFSTARTUP_FULL = 0x00070;
    const uint MF_SOURCE_READER_FIRST_VIDEO_STREAM = 0xFFFFFFFE;
    const uint MF_SOURCE_READER_MEDIASOURCE = 0xFFFFFFFF;
    const uint MF_SOURCE_READER_CURRENT_TYPE_INDEX = 0xFFFFFFFF;
    const uint MF_SINK_WRITER_INPUT_STREAM = 0xFFFFFFFD;

    [DllImport("mfplat.dll")] static extern int MFStartup(uint Version, uint dwFlags);
    [DllImport("mfplat.dll")] static extern int MFShutdown();
    [DllImport("mfreadwrite.dll")] static extern int MFCreateSourceReaderFromURL(
        [MarshalAs(UnmanagedType.LPWStr)] string pwszURL, IntPtr pAttributes, out IntPtr ppSourceReader);
    [DllImport("mfreadwrite.dll")] static extern int MFCreateSinkWriterFromURL(
        [MarshalAs(UnmanagedType.LPWStr)] string pwszOutputURL, IntPtr pByteStream, IntPtr pAttributes, out IntPtr ppSinkWriter);
    [DllImport("mfreadwrite.dll")] static extern int MFCreateAttributes(out IntPtr ppMFAttributes, uint cInitialSize);

    static Guid MF_MT_MAJOR_TYPE           = new Guid("48eba18e-f8c9-4687-bf11-0a74c9f96a8f");
    static Guid MF_MT_SUBTYPE              = new Guid("f7e34c9a-42e8-4714-b74b-cb29d72b35ad");
    static Guid MF_MT_FRAME_SIZE           = new Guid("1652c33d-d6b2-4012-b834-72030849a37d");
    static Guid MF_MT_FRAME_RATE           = new Guid("c459a2e8-3d2c-4e44-b132-fee5156c7bb0");
    static Guid MF_MT_AVG_BITRATE          = new Guid("20332624-fb0d-4d9e-bd0d-cbf6786c102e");
    static Guid MF_MT_INTERLACE_MODE       = new Guid("e2724bb8-e676-480b-b2b2-a8d6efb44ccd");
    static Guid MFMediaType_Video          = new Guid("73646976-0000-0010-8000-00AA00389B71");
    static Guid MFVideoFormat_H264         = new Guid("34363248-0000-0010-8000-00AA00389B71");
    static Guid MF_MT_MPEG2_PROFILE        = new Guid("ad76a80b-2d5c-4e0b-b375-64e520137036");
    
    [DllImport("ole32.dll")] static extern int PropVariantClear(IntPtr pvar);

    [StructLayout(LayoutKind.Sequential)]
    struct PROPVARIANT { public ushort vt; ushort wReserved1; ushort wReserved2; ushort wReserved3; public IntPtr unionData; }

    [DllImport("mfreadwrite.dll")] static extern int IMFSourceReader_SetCurrentMediaType(
        IntPtr pSourceReader, uint dwStreamIndex, IntPtr pdwReserved, IntPtr pMediaType);
    [DllImport("mfreadwrite.dll")] static extern int IMFSourceReader_GetCurrentMediaType(
        IntPtr pSourceReader, uint dwStreamIndex, out IntPtr ppMediaType);
    [DllImport("mfreadwrite.dll")] static extern int IMFSourceReader_ReadSample(
        IntPtr pSourceReader, uint dwStreamIndex, uint dwControlFlags, out uint pdwActualStreamIndex,
        out uint pdwStreamFlags, out long pllTimestamp, out IntPtr ppSample);
    [DllImport("mfreadwrite.dll")] static extern int IMFSinkWriter_AddStream(
        IntPtr pSinkWriter, IntPtr pTargetMediaType, out uint pdwStreamIndex);
    [DllImport("mfreadwrite.dll")] static extern int IMFSinkWriter_SetInputMediaType(
        IntPtr pSinkWriter, uint dwStreamIndex, IntPtr pInputMediaType, IntPtr pEncodingParameters);
    [DllImport("mfreadwrite.dll")] static extern int IMFSinkWriter_WriteSample(
        IntPtr pSinkWriter, uint dwStreamIndex, IntPtr pSample);
    [DllImport("mfreadwrite.dll")] static extern int IMFSinkWriter_BeginWriting(IntPtr pSinkWriter);
    [DllImport("mfreadwrite.dll")] static extern int IMFSinkWriter_Finalize(IntPtr pSinkWriter);
    [DllImport("mfplat.dll")] static extern int MFCreateMediaType(out IntPtr ppMFType);
    [DllImport("mfplat.dll")] static extern int IMFMediaType_SetGUID(IntPtr pMediaType, ref Guid guidKey, ref Guid guidValue);
    [DllImport("mfplat.dll")] static extern int IMFMediaType_SetUINT32(IntPtr pMediaType, ref Guid guidKey, uint unValue);
    [DllImport("mfplat.dll")] static extern int IMFMediaType_SetUINT64(IntPtr pMediaType, ref Guid guidKey, ulong unValue);
    [DllImport("mfplat.dll")] static extern int MFSetAttributeSize(IntPtr pAttributes, ref Guid guidKey, uint unWidth, uint unHeight);
    [DllImport("mfplat.dll")] static extern int MFSetAttributeRatio(IntPtr pAttributes, ref Guid guidKey, uint unNumerator, uint unDenominator);
    
    [DllImport("kernel32.dll")] static extern IntPtr GetConsoleWindow();

    public static string Compress(string inputPath, string outputPath, int targetWidth, int targetHeight, uint bitrate)
    {
        int hr = MFStartup(MFSTARTUP_FULL);
        if (hr < 0) return "MFStartup failed: 0x" + hr.ToString("X8");

        try
        {
            // Create source reader
            IntPtr pSourceReader;
            hr = MFCreateSourceReaderFromURL(inputPath, IntPtr.Zero, out pSourceReader);
            if (hr < 0) return "SourceReader create failed: 0x" + hr.ToString("X8");

            // Get source video media type  
            IntPtr pSourceType;
            hr = IMFSourceReader_GetCurrentMediaType(pSourceReader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, out pSourceType);
            if (hr < 0) return "GetMediaType failed: 0x" + hr.ToString("X8");

            // Create output media type (H.264)
            IntPtr pOutputType;
            hr = MFCreateMediaType(out pOutputType);
            if (hr < 0) return "CreateMediaType failed: 0x" + hr.ToString("X8");

            hr = IMFMediaType_SetGUID(pOutputType, ref MF_MT_MAJOR_TYPE, ref MFMediaType_Video);
            hr = IMFMediaType_SetGUID(pOutputType, ref MF_MT_SUBTYPE, ref MFVideoFormat_H264);
            hr = MFSetAttributeSize(pOutputType, ref MF_MT_FRAME_SIZE, (uint)targetWidth, (uint)targetHeight);
            hr = MFSetAttributeRatio(pOutputType, ref MF_MT_FRAME_RATE, 24000, 1000); // 24fps
            hr = IMFMediaType_SetUINT32(pOutputType, ref MF_MT_AVG_BITRATE, bitrate);
            hr = IMFMediaType_SetUINT32(pOutputType, ref MF_MT_INTERLACE_MODE, 2); // Progressive
            hr = IMFMediaType_SetUINT32(pOutputType, ref MF_MT_MPEG2_PROFILE, 77); // High profile

            // Set source reader output type
            hr = IMFSourceReader_SetCurrentMediaType(pSourceReader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, IntPtr.Zero, pOutputType);

            // Create sink writer
            IntPtr pSinkWriter;
            hr = MFCreateSinkWriterFromURL(outputPath, IntPtr.Zero, IntPtr.Zero, out pSinkWriter);
            if (hr < 0) return "SinkWriter create failed: 0x" + hr.ToString("X8");

            uint streamIndex;
            hr = IMFSinkWriter_AddStream(pSinkWriter, pOutputType, out streamIndex);
            if (hr < 0) return "AddStream failed: 0x" + hr.ToString("X8");

            hr = IMFSinkWriter_SetInputMediaType(pSinkWriter, streamIndex, pOutputType, IntPtr.Zero);
            if (hr < 0) return "SetInputMediaType failed: 0x" + hr.ToString("X8");

            hr = IMFSinkWriter_BeginWriting(pSinkWriter);
            if (hr < 0) return "BeginWriting failed: 0x" + hr.ToString("X8");

            // Transcode loop
            int frameCount = 0;
            while (true)
            {
                uint actualStream, flags;
                long timestamp;
                IntPtr pSample;
                hr = IMFSourceReader_ReadSample(pSourceReader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                    out actualStream, out flags, out timestamp, out pSample);
                
                if (hr < 0 || (flags & 0x8000) != 0) // MF_SOURCE_READERF_ENDOFSTREAM = 0x8000
                    break;

                if (pSample != IntPtr.Zero)
                {
                    hr = IMFSinkWriter_WriteSample(pSinkWriter, streamIndex, pSample);
                    Marshal.Release(pSample);
                }
                frameCount++;
            }

            hr = IMFSinkWriter_Finalize(pSinkWriter);
            
            Marshal.Release(pSinkWriter);
            Marshal.Release(pSourceReader);
            Marshal.Release(pOutputType);
            Marshal.Release(pSourceType);

            return "OK: " + frameCount + " frames written";
        }
        finally
        {
            MFShutdown();
        }
    }
}
"@ -ReferencedAssemblies "System.Runtime.InteropServices"

Write-Host "Original video: 4474x2654, 12s, ~21MB"
Write-Host "Compressing to 1280x720 @ 2Mbps..."

$input  = "e:\UE_Project\UE5GameFramework\GameFrameworkDev\Content\Movies\Login.mp4"
$output = "e:\UE_Project\UE5GameFramework\GameFrameworkDev\Content\Movies\Login_compressed.mp4"

# Target: 1280x720, 2Mbps = ~3MB for 12 seconds
$result = [MFTools]::Compress($input, $output, 1280, 720, 2000000)
Write-Host "Result: $result"

if ($result.StartsWith("OK")) {
    $size = (Get-Item $output).Length / 1MB
    Write-Host "Compressed size: $([math]::Round($size, 2)) MB"
}
