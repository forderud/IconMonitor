using System.IO.Pipes;
using System.Text;

class PipeClient
{
    static void Main(string[] args)
    {
        try
        {
            var pipeClient = new NamedPipeClientStream(".", "NamedPipe/Test", PipeDirection.InOut, PipeOptions.None);
            pipeClient.Connect(100);

            Console.WriteLine("Connected");

            for (int i = 0; i < 10; ++i) {
                string message = "Hello world!";
                pipeClient.Write(Encoding.Default.GetBytes(message));
                Thread.Sleep(1000);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine("Failed: " + ex);
            return;
        }
    }
}
