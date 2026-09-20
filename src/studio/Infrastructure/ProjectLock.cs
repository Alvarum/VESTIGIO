using System.Security.Cryptography;
using System.Text;

namespace RetroForge.Studio;

/// <summary>
/// Bloqueo local por manifiesto. Todas las ventanas desacopladas pertenecen al
/// mismo proceso; un segundo proceso no debe escribir el mismo proyecto con un
/// historial distinto.
/// </summary>
internal sealed class ProjectLock : IDisposable
{
    private readonly Mutex _mutex;
    private bool _ownsMutex;

    private ProjectLock(Mutex mutex, bool ownsMutex)
    {
        _mutex = mutex;
        _ownsMutex = ownsMutex;
    }

    public static ProjectLock Acquire(string manifest)
    {
        string canonical = Path.GetFullPath(manifest).ToUpperInvariant();
        string hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(canonical)))[..20];
        var mutex = new Mutex(initiallyOwned: true, $"Local\\RetroForge.Studio.{hash}", out bool created);
        bool owns = created;
        if (!created)
        {
            try
            {
                owns = mutex.WaitOne(0);
            }
            catch (AbandonedMutexException)
            {
                owns = true;
            }
        }
        if (!owns)
        {
            mutex.Dispose();
            throw new InvalidOperationException(
                "Este proyecto ya está abierto para edición en otra ventana de RetroForge Studio.");
        }
        return new ProjectLock(mutex, owns);
    }

    public void Dispose()
    {
        if (_ownsMutex)
        {
            _mutex.ReleaseMutex();
            _ownsMutex = false;
        }
        _mutex.Dispose();
    }
}
