#pragma once

// Previous release solver, retained only as a regression oracle.
namespace route_reference
{
using practice::RouteResult;
using practice::Cancel;
using practice::Unreachable;

inline RouteResult SolvePreviousOrder(
    const std::vector<std::vector<double>> &distances, const Cancel &canceled = [] { return false; },
    bool exactWhenPossible = true)
{
    RouteResult result;
    int n = static_cast<int>(distances.size()) - 1;
    if (n <= 0)
        return result;
    for (const auto &row : distances)
        for (double value : row)
            if (!std::isfinite(value))
            {
                result.message = "Some crystals are unreachable on the captured NavMesh; no complete route.";
                return result;
            }
    if (n <= 15 && exactWhenPossible)
    {
        result.exact = true;
        int masks = 1 << n;
        std::vector<double> dp(static_cast<size_t>(masks) * n, Unreachable);
        std::vector<int8_t> parent(dp.size(), -1);
        for (int end = 0; end < n; end++)
            dp[(1 << end) * n + end] = distances[0][end + 1];
        for (int mask = 1; mask < masks; mask++)
        {
            if ((mask & 255) == 0 && canceled())
                throw std::runtime_error("Canceled");
            for (int end = 0; end < n; end++)
                if (mask & (1 << end))
                {
                    int previous = mask ^ (1 << end);
                    if (!previous)
                        continue;
                    for (int from = 0; from < n; from++)
                        if (previous & (1 << from))
                        {
                            double candidate = dp[previous * n + from] + distances[from + 1][end + 1];
                            if (candidate < dp[mask * n + end])
                            {
                                dp[mask * n + end] = candidate;
                                parent[mask * n + end] = static_cast<int8_t>(from);
                            }
                        }
                }
        }
        int end = 0;
        for (int i = 1; i < n; i++)
            if (dp[(masks - 1) * n + i] < dp[(masks - 1) * n + end])
                end = i;
        result.length = dp[(masks - 1) * n + end];
        int mask = masks - 1;
        while (mask)
        {
            result.order.push_back(end);
            int previous = parent[mask * n + end];
            mask ^= 1 << end;
            end = previous;
        }
        std::reverse(result.order.begin(), result.order.end());
    }
    else
    {
        result.length = Unreachable;
        auto length = [&](const std::vector<int> &order) {
            double value = 0;
            int from = 0;
            for (int id : order)
            {
                value += distances[from][id + 1];
                from = id + 1;
            }
            return value;
        };
        for (int first = 0; first < n; first++)
        {
            if (canceled())
                throw std::runtime_error("Canceled");
            std::vector<int> order{first};
            std::vector<bool> used(n, false);
            used[first] = true;
            while (static_cast<int>(order.size()) < n)
            {
                int next = -1;
                for (int i = 0; i < n; i++)
                    if (!used[i] && (next < 0 || distances[order.back() + 1][i + 1] <
                                                     distances[order.back() + 1][next + 1]))
                        next = i;
                order.push_back(next);
                used[next] = true;
            }
            // Open-path 2-opt, including reversal of the final section.
            double current = length(order);
            for (int pass = 0; pass < 32; pass++)
            {
                if (canceled())
                    throw std::runtime_error("Canceled");
                bool improved = false;
                for (int a = 0; a < n - 1; a++)
                    for (int b = a + 1; b < n; b++)
                    {
                        int prev = a ? order[a - 1] + 1 : 0, next = b + 1 < n ? order[b + 1] + 1 : -1;
                        double delta = distances[prev][order[b] + 1] - distances[prev][order[a] + 1];
                        if (next >= 0)
                            delta += distances[order[a] + 1][next] - distances[order[b] + 1][next];
                        if (delta < -.000001)
                        {
                            std::reverse(order.begin() + a, order.begin() + b + 1);
                            current += delta;
                            improved = true;
                        }
                    }
                if (!improved)
                    break;
            }
            if (current < result.length)
            {
                result.length = current;
                result.order = order;
            }
        }
    }
    result.complete = true;
    result.message = result.exact ? "Exact visit order for the modeled NavMesh distances (up to 15 crystals)."
                                  : "Multi-start nearest neighbor + 2-opt; approximate visit order.";
    return result;
}
} // namespace route_reference
