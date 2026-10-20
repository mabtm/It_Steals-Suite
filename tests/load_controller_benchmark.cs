using System;
public static class LoadControllerBenchmark
{
    public static void Main()
    {
        const int frames=7200;const double frameMs=1000.0/120,seedMs=.00094;
        var controller=new SearchLoadController(true,2,frameMs,0);double previousWork=0;long modeled=0;
        for(int i=1;i<=frames;i++){
            bool ready=i%2==0;controller.Observe(frameMs,previousWork,ready,i/120.0);previousWork=0;
            if(ready){int samples=Math.Min(2048,(int)(controller.Milliseconds/seedMs));modeled+=samples;previousWork=samples*seedMs;}
        }
        Console.WriteLine("steady 120 FPS, ACK every second frame: limit={0:F4} ms, modeled={1:F0} layouts/s",controller.Milliseconds,modeled/60.0);
        Console.WriteLine("Controller replay only; assumes 0.94 microseconds per layout from the screenshot, not a measured Unity rate.");
    }
}
