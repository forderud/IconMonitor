using System.IO.Pipes;


class PipeClient
{
    static void Main(string[] args)
    {
        try
        {
            var pipeClient = new NamedPipeClientStream(".", "NamedPipe/Test",
                PipeDirection.InOut,
                PipeOptions.None);
            pipeClient.Connect(100);
        }
        catch (Exception ex)
        {
            Console.WriteLine("Failed: " + ex);
            return;
        }

        Console.WriteLine("Connected");
        Console.ReadLine();
    }
}
