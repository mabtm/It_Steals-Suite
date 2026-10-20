using System;
using System.Collections.Generic;
using System.Diagnostics;
using UnityEngine;
using UnityEngine.AI;
using UnityEngine.SceneManagement;

// A compact union of corridors returned by Unity, not a guessed wall/grid map.
// Pure bookkeeping is independently tested. All Unity queries remain in Tick.
public sealed class NavigationGraphBuilder
{
    public const int MaximumNodes = 10000, MaximumEdges = 30000;

    private struct Key : IEquatable<Key>
    {
        public long X, Y, Z;

        public bool Equals(Key other)
        {
            return X == other.X && Y == other.Y && Z == other.Z;
        }

        public override bool Equals(object other)
        {
            return other is Key && Equals((Key)other);
        }

        public override int GetHashCode()
        {
            unchecked
            {
                return X.GetHashCode() * 73856093 ^ Y.GetHashCode() * 19349663 ^ Z.GetHashCode() * 83492791;
            }
        }
    }

    private readonly Dictionary<Key, int> ids = new Dictionary<Key, int>();
    private readonly List<Vector3> nodes = new List<Vector3>();
    private readonly HashSet<ulong> knownEdges = new HashSet<ulong>(), blockedEdges = new HashSet<ulong>();
    private readonly List<int> edges = new List<int>();

    public int AddNode(Vector3 p)
    {
        if (float.IsNaN(p.x) || float.IsInfinity(p.x) || float.IsNaN(p.y) || float.IsInfinity(p.y) ||
            float.IsNaN(p.z) || float.IsInfinity(p.z) || Math.Abs(p.x) > 100000 || Math.Abs(p.y) > 100000 ||
            Math.Abs(p.z) > 100000)
            throw new InvalidOperationException("Invalid Unity navigation coordinate");
        var key = new Key { X = (long)Math.Round(p.x * 10000.0), Y = (long)Math.Round(p.y * 10000.0),
                            Z = (long)Math.Round(p.z * 10000.0) };
        int id;
        if (ids.TryGetValue(key, out id))
            return id;
        if (nodes.Count >= MaximumNodes)
            throw new InvalidOperationException("Unity corridor node limit reached");
        id = nodes.Count;
        nodes.Add(p);
        ids.Add(key, id);
        return id;
    }

    public void AddEdge(int a, int b, Func<Vector3, Vector3, bool> walkable)
    {
        if (a < 0 || b < 0 || a >= nodes.Count || b >= nodes.Count)
            throw new ArgumentOutOfRangeException("Corridor endpoint");
        if (a == b)
            return;
        if (a > b)
        {
            int swap = a;
            a = b;
            b = swap;
        }
        ulong key = ((ulong)(uint)a << 32) | (uint)b;
        if (knownEdges.Contains(key) || blockedEdges.Contains(key))
            return;
        // Off-mesh links/holes are not silently made bidirectional shortcuts.
        if (!walkable(nodes[a], nodes[b]))
        {
            if (blockedEdges.Count < MaximumEdges)
                blockedEdges.Add(key);
            return;
        }
        if (edges.Count / 2 >= MaximumEdges)
            throw new InvalidOperationException("Unity corridor edge limit reached");
        knownEdges.Add(key);
        edges.Add(a);
        edges.Add(b);
    }

    private static bool CompletePathSegment(Vector3 a, Vector3 b)
    {
        return true;
    }

    private static readonly Func<Vector3, Vector3, bool> completePathSegment = CompletePathSegment;

    public void AddCompletePath(int from, int to, Vector3[] corners, int count)
    {
        AddCompletePath(from, to, corners, count, completePathSegment);
    }

    public bool AddCompletePath(int from, int to, Vector3[] corners, int count,
                                Func<Vector3, Vector3, bool> walkable)
    {
        if (from < 0 || to < 0 || from >= nodes.Count || to >= nodes.Count || count < 1 ||
            count > corners.Length)
            throw new ArgumentOutOfRangeException("Complete corridor");
        if ((corners[0] - nodes[from]).sqrMagnitude > .0004f ||
            (corners[count - 1] - nodes[to]).sqrMagnitude > .0004f)
            throw new InvalidOperationException("Complete path endpoints do not match the catalog");
        // Validate canonical endpoint connectors as well as intermediate
        // corners before publishing ANY edges from this path.
        Vector3 at = nodes[from];
        for (int i = 1; i < count; i++)
        {
            Vector3 next = i == count - 1 ? nodes[to] : corners[i];
            if (!walkable(at, next))
                return false;
            at = next;
        }
        if (count == 1 && !walkable(nodes[from], nodes[to]))
            return false;
        // CalculatePath(PathComplete) already validates these corridor legs.
        // A second Raycast from a boundary corner can report a zero-distance
        // hit and sever an otherwise complete path. Preserve the returned
        // path and bind its endpoints to the catalog's canonical nodes.
        int previous = from;
        for (int i = 1; i < count - 1; i++)
        {
            int next = AddNode(corners[i]);
            AddEdge(previous, next, completePathSegment);
            previous = next;
        }
        AddEdge(previous, to, completePathSegment);
        return true;
    }

    public Vector3[] Nodes()
    {
        return nodes.ToArray();
    }

    public int[] Edges()
    {
        return edges.ToArray();
    }
}

public sealed class UnityNavigationCapture
{
    public const int MaximumCatalog = 256;
    private readonly PracticeMapData data;
    private readonly Vector3[] projected;
    private readonly int[] anchors;
    private readonly NavigationGraphBuilder graph = new NavigationGraphBuilder();
    private readonly NavMeshPath path;
    private NavMeshQueryFilter filter;
    private readonly Vector3[] corners = new Vector3[1024];
    private int projectAt, from, to, completeQueries;
    private bool done;

    public UnityNavigationCapture(PracticeMapData catalog, MouseLook player = null, bool recorded = false)
    {
        data = catalog;
        int count = data.Spawns.Length + data.Points.Length;
        data.NavigationModel = 0;
        if (count < 1 || count > MaximumCatalog)
            throw new InvalidOperationException(
                "Unity corridor capture supports up to 256 catalog positions");
        int type = 0, scene = SceneManager.GetActiveScene().handle;
        bool surfaceFound = false;
        // The scene's loaded surface defines the available navigation data.
        // Outside loads a different type from the spider/disabled classic AI.
        foreach (var surface in NavMeshSurface.activeSurfaces)
            if (surface != null && surface.isActiveAndEnabled && surface.gameObject.scene.handle == scene &&
                surface.navMeshData != null)
            {
                type = surface.agentTypeID;
                surfaceFound = true;
                break;
            }
        if (!surfaceFound)
            foreach (NavMeshAgent agent in UnityEngine.Object.FindObjectsOfType<NavMeshAgent>())
                if (agent.enabled && agent.gameObject.scene.handle == scene)
                {
                    type = agent.agentTypeID;
                    break;
                }
        path = new NavMeshPath();
        filter = new NavMeshQueryFilter { agentTypeID = type, areaMask = NavMesh.AllAreas };
        projected = new Vector3[count];
        anchors = new int[count];
        for (int i = 0; i < count; i++)
            anchors[i] = -1;
        data.NavigationStatus = 1;
        data.Flags |= 128;
        data.NavigationQueries = 0;
        data.NavigationTotal =
            data.Spawns.Length * data.Points.Length + data.Points.Length * (data.Points.Length - 1) / 2;
        to = data.Spawns.Length;
        data.NavigationNote = "Capturing actual Unity navigation corridors.";
        data.NavigationModel = 0;
    }

    public bool Tick()
    {
        if (done)
            return true;
        long start = Stopwatch.GetTimestamp(), limit = Math.Max(1, Stopwatch.Frequency / 1000);
        do
        {
            if (projectAt < anchors.Length)
            {
                Vector3 original = projectAt < data.Spawns.Length
                                       ? data.Spawns[projectAt]
                                       : data.Points[projectAt - data.Spawns.Length];
                NavMeshHit hit;
                if (NavMesh.SamplePosition(original, out hit, 4, filter))
                {
                    Vector3 delta = hit.position - original;
                    if (delta.x * delta.x + delta.z * delta.z <= 4.0001f && Math.Abs(delta.y) <= 4)
                    {
                        projected[projectAt] = hit.position;
                        anchors[projectAt] = graph.AddNode(hit.position);
                    }
                }
                projectAt++;
            }
            else if (from < anchors.Length)
            {
                if (to >= anchors.Length)
                {
                    from++;
                    to = Math.Max(data.Spawns.Length, from + 1);
                    continue;
                }
                if (anchors[from] >= 0 && anchors[to] >= 0 &&
                    NavMesh.CalculatePath(projected[from], projected[to], filter, path) &&
                    path.status == NavMeshPathStatus.PathComplete)
                {
                    int count = path.GetCornersNonAlloc(corners);
                    if (count > 0 && count < corners.Length &&
                        (corners[0] - projected[from]).sqrMagnitude <= .0004f &&
                        (corners[count - 1] - projected[to]).sqrMagnitude <= .0004f)
                    {
                        completeQueries++;
                        graph.AddCompletePath(anchors[from], anchors[to], corners, count);
                    }
                }
                data.NavigationQueries++;
                to++;
            }
            else
            {
                data.NavigationNodes = graph.Nodes();
                data.NavigationEdges = graph.Edges();
                data.NavigationAnchors = anchors;
                data.NavigationStatus = 2;
                data.Flags &= ~128;
                int valid = 0;
                foreach (int anchor in anchors)
                    if (anchor >= 0)
                        valid++;
                data.NavigationNote =
                    "Unity agent type " + filter.agentTypeID + ": " + valid + "/" + anchors.Length +
                    " anchors, " + completeQueries + " complete queries, " + data.NavigationEdges.Length / 2 +
                    " corridor edges. This is an AI navigation model; player movement, off-mesh actions and dynamic obstacles are not verified.";
                done = true;
                path.ClearCorners();
                return true;
            }
        } while (Stopwatch.GetTimestamp() - start < limit);
        return false;
    }

    public void Cancel()
    {
        done = true;
        path.ClearCorners();
        data.Flags &= ~128;
        if (data.NavigationStatus == 1)
        {
            data.NavigationStatus = 0;
            data.NavigationModel = 0;
            data.NavigationNote = "Reload this level to capture Unity navigation.";
        }
    }
}
