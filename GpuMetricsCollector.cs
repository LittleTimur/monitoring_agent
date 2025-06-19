using System;
using System.Text.Json;
using System.Collections.Generic;
using LibreHardwareMonitor.Hardware;

class GpuMetrics
{
    public double? Temperature { get; set; }
    public double? UsagePercent { get; set; }
    public double? MemoryUsed { get; set; }
    public double? MemoryTotal { get; set; }
    public string Name { get; set; }
}

class Program
{
    static void Main()
    {
        var computer = new Computer { IsGpuEnabled = true };
        computer.Open();
        var result = new List<GpuMetrics>();
        foreach (var hardware in computer.Hardware)
        {
            if (hardware.HardwareType == HardwareType.GpuNvidia ||
                hardware.HardwareType == HardwareType.GpuAmd ||
                hardware.HardwareType == HardwareType.GpuIntel)
            {
                hardware.Update();
                var metrics = new GpuMetrics { Name = hardware.Name };
                foreach (var sensor in hardware.Sensors)
                {
                    if (sensor.SensorType == SensorType.Temperature && metrics.Temperature == null)
                        metrics.Temperature = sensor.Value;
                    if (sensor.SensorType == SensorType.Load && sensor.Name.Contains("Core") && metrics.UsagePercent == null)
                        metrics.UsagePercent = sensor.Value;
                    if (sensor.SensorType == SensorType.SmallData && sensor.Name.Contains("Memory Used"))
                        metrics.MemoryUsed = sensor.Value;
                    if (sensor.SensorType == SensorType.SmallData && sensor.Name.Contains("Memory Total"))
                        metrics.MemoryTotal = sensor.Value;
                }
                result.Add(metrics);
            }
        }
        // Выводим только первую GPU (можно изменить при необходимости)
        if (result.Count > 0)
            Console.WriteLine(JsonSerializer.Serialize(result[0]));
        else
            Console.WriteLine("{}\n");
    }
} 