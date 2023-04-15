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

        NamedPipeServerStream pipeServer = (NamedPipeServerStream)ar.AsyncState;

        while (true) {
            byte[] buffer = new byte[255];
            int res = pipeServer.Read(buffer);
            if (res == 0)
                return;
            Console.WriteLine(Encoding.Default.GetString(buffer));
        }
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
            const string LOW_INTEGRITY_LABEL_SACL = "S:(ML;;NW;;;LW)";
            const string EVERYONE_CLIENT_ACE = "D:(A;;0x12019b;;;WD)";

            IntPtr securityDescriptorPtr = IntPtr.Zero;
            int securityDescriptorSize = 0;
            bool result = ConvertStringSecurityDescriptorToSecurityDescriptor(
                LOW_INTEGRITY_LABEL_SACL + EVERYONE_CLIENT_ACE, 1, out securityDescriptorPtr, out securityDescriptorSize);
            if (!result)
                throw new Win32Exception(Marshal.GetLastWin32Error());

            SECURITY_ATTRIBUTES securityAttributes = new SECURITY_ATTRIBUTES();
            securityAttributes.nLength = Marshal.SizeOf(securityAttributes);
            securityAttributes.bInheritHandle = 1;
            securityAttributes.lpSecurityDescriptor = securityDescriptorPtr;

            int openMode = (int)PipeDirection.InOut | (int)PipeOptions.Asynchronous;
            int pipeMode = 4 + 2 + 0; // PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT
            int maxNumberOfServerInstances = 255;
            int inBufferSize = 0;
            int outBufferSize = 0;
            SafePipeHandle handle = CreateNamedPipe(@"\\.\pipe\" + pipeName, openMode, pipeMode,
                maxNumberOfServerInstances, outBufferSize, inBufferSize, 0, securityAttributes);
            if (handle.IsInvalid)
                throw new Win32Exception(Marshal.GetLastWin32Error());
            return handle;
        }
    }
}
