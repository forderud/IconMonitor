using Microsoft.Win32.SafeHandles;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.ComponentModel;
using System.Security.Principal;
using System.Text;

class PipeServer
{
    static void Main(string[] args)
    {
        SafePipeHandle handle = LowIntegrityPipeFactory.CreateLowIntegrityNamedPipe("NamedPipe/Test");
        NamedPipeServerStream pipeServer = new NamedPipeServerStream(PipeDirection.InOut, true, false, handle);
        pipeServer.BeginWaitForConnection(HandleConnection, pipeServer);

        Console.WriteLine("Waiting...");
        Console.ReadLine();
    }

    private static void HandleConnection(IAsyncResult ar)
    {
        Console.WriteLine("Received connection");
    }


    static class LowIntegrityPipeFactory
    {
        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern SafePipeHandle CreateNamedPipe(string pipeName, int openMode,
            int pipeMode, int maxInstances, int outBufferSize, int inBufferSize, int defaultTimeout,
            SECURITY_ATTRIBUTES securityAttributes);

        [DllImport("Advapi32.dll", CharSet = CharSet.Auto, SetLastError = true, ExactSpelling = false)]
        private static extern bool ConvertStringSecurityDescriptorToSecurityDescriptor(
            [In] string StringSecurityDescriptor,
            [In] uint StringSDRevision,
            [Out] out IntPtr SecurityDescriptor,
            [Out] out int SecurityDescriptorSize
        );

        [StructLayout(LayoutKind.Sequential)]
        private struct SECURITY_ATTRIBUTES
        {
            public int nLength;
            public IntPtr lpSecurityDescriptor;
            public int bInheritHandle;
        }

        public static SafePipeHandle CreateLowIntegrityNamedPipe(string pipeName)
        {
            // convert the security descriptor
            IntPtr securityDescriptorPtr = IntPtr.Zero;
            int securityDescriptorSize = 0;
            bool result = ConvertStringSecurityDescriptorToSecurityDescriptor(
                CreateSddlForPipeSecurity(), 1, out securityDescriptorPtr, out securityDescriptorSize);
            if (!result)
                throw new Win32Exception(Marshal.GetLastWin32Error());

            SECURITY_ATTRIBUTES securityAttributes = new SECURITY_ATTRIBUTES();
            securityAttributes.nLength = Marshal.SizeOf(securityAttributes);
            securityAttributes.bInheritHandle = 1;
            securityAttributes.lpSecurityDescriptor = securityDescriptorPtr;

            SafePipeHandle handle = CreateNamedPipe(@"\\.\pipe\" + pipeName,
                PipeDirection.InOut, 100, PipeTransmissionMode.Byte, PipeOptions.Asynchronous,
                0, 0, PipeAccessRights.ReadWrite, securityAttributes);
            if (handle.IsInvalid)
                throw new Win32Exception(Marshal.GetLastWin32Error());

            return handle;
        }

        private static SafePipeHandle CreateNamedPipe(string fullPipeName, PipeDirection direction,
            int maxNumberOfServerInstances, PipeTransmissionMode transmissionMode, PipeOptions options,
            int inBufferSize, int outBufferSize, PipeAccessRights rights, SECURITY_ATTRIBUTES secAttrs)
        {
            int openMode = (int)direction | (int)options;
            int pipeMode = 0;
            if (maxNumberOfServerInstances == -1)
                maxNumberOfServerInstances = 0xff;

            SafePipeHandle handle = CreateNamedPipe(fullPipeName, openMode, pipeMode,
                maxNumberOfServerInstances, outBufferSize, inBufferSize, 0, secAttrs);
            if (handle.IsInvalid)
                throw new Win32Exception(Marshal.GetLastWin32Error());
            return handle;
        }

        private static string CreateSddlForPipeSecurity()
        {
            const string LOW_INTEGRITY_LABEL_SACL = "S:(ML;;NW;;;LW)";
            const string EVERYONE_CLIENT_ACE = "(A;;0x12019b;;;WD)";
            const string CALLER_ACE_TEMPLATE = "(A;;0x12019f;;;{0})";

            StringBuilder sb = new StringBuilder();
            sb.Append(LOW_INTEGRITY_LABEL_SACL);
            sb.Append("D:"+EVERYONE_CLIENT_ACE);
            //sb.AppendFormat(CALLER_ACE_TEMPLATE, WindowsIdentity.GetCurrent().Owner.Value);
            return sb.ToString();
        }

    }
}
