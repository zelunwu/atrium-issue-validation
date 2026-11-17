using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Text.Json;

namespace Atrium.Settings;

// Deterministic stand-ins for the preview renderer's photo and settings inputs.
internal sealed class Photo
{
    public static Photo Default { get; } = new();

    public void Paint(byte[] pixels, int width, int height, float artW, float artH, float x0, float y0, float ppd)
    {
        for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            int i = 4 * (y * width + x);
            int stripe = (x / 13) % 2 == 0 ? 35 : 215;
            int checker = ((x / 29) + (y / 29)) % 2 == 0 ? 35 : 0;
            pixels[i] = (byte)Math.Min(255, stripe + checker);
            pixels[i + 1] = (byte)((y * 3 + checker) % 256);
            pixels[i + 2] = (byte)((x + y + stripe / 2) % 256);
            pixels[i + 3] = 255;
        }
    }
}

internal sealed class BarSettings
{
    public int Tint { get; init; } = 0xFFFFFF;
    public double CornerRadius { get; init; } = 28;
    public double Edge { get; init; } = 24;
    public double Refraction { get; init; } = 18;
    public double Dispersion { get; init; } = 0.05;
    public double Blur { get; init; } = 0.14;
    public double Transparency { get; init; } = 0.8;
    public double Highlight { get; init; } = 0.5;
    public double Shadow { get; init; } = 0.25;
}

internal static class Program
{
    private static int Main(string[] args)
    {
        if (args.Length != 1)
        {
            Console.Error.WriteLine("Usage: AtriumVisualBaseline <output-directory>");
            return 2;
        }

        string output = Path.GetFullPath(args[0]);
        Directory.CreateDirectory(output);
        const int width = 640, height = 320;
        var backdrop = Backdrop.Create(width, height, width, height, 0, 0, 1);
        var settings = new BarSettings();
        var cases = new (string Name, GlassSpec Spec)[]
        {
            ("liquid", GlassSpec.For(settings, 1)),
            ("frosted", GlassSpec.For(WithBlur(0.25), 1)),
            ("pill", GlassSpec.For(settings, 1) with { Radius = 110, Edge = 32, Refract = 24 })
        };

        var timings = new Dictionary<string, double>();
        foreach (var (name, spec) in cases)
        {
            byte[] Render()
            {
                byte[] pixels = (byte[])backdrop.Base.Clone();
                GlassRenderer.Draw(backdrop, pixels, 70, 60, 500, 200, spec);
                return pixels;
            }

            _ = Render(); // JIT warmup
            var samples = new double[5];
            byte[]? final = null;
            for (int n = 0; n < samples.Length; n++)
            {
                var watch = Stopwatch.StartNew();
                final = Render();
                watch.Stop();
                samples[n] = watch.Elapsed.TotalMilliseconds;
            }
            Array.Sort(samples);
            timings[name] = samples[2];
            WritePng(Path.Combine(output, name + ".png"), final!, width, height);
            Console.WriteLine($"{name}: median CPU preview render {samples[2]:F2} ms");
        }

        File.WriteAllText(Path.Combine(output, "timings.json"), JsonSerializer.Serialize(new
        {
            renderer = "Atrium.Settings.GlassPreview CPU port",
            size = $"{width}x{height}",
            unit = "milliseconds",
            statistic = "median of 5 runs after 1 warmup; includes buffer clone",
            measurements = timings
        }, new JsonSerializerOptions { WriteIndented = true }));
        return 0;
    }

    private static BarSettings WithBlur(double blur) => new() { Blur = blur };

    private static void WritePng(string path, byte[] bgra, int width, int height)
    {
        using var bitmap = new Bitmap(width, height, PixelFormat.Format32bppArgb);
        var area = bitmap.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        try { Marshal.Copy(bgra, 0, area.Scan0, bgra.Length); }
        finally { bitmap.UnlockBits(area); }
        bitmap.Save(path, ImageFormat.Png);
    }
}
