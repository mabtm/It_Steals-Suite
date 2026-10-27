#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <span>
#include <stdexcept>
#include <vector>

namespace practice
{
// Experimental lower bound, not enabled in the production search.
// Each interior node chooses two distinct neighbors at ONE contact position.
// Spawn and one unknown endpoint have degree one. Half the sum of these local
// incident costs is <= every complete open route in the same contact model.
class ContactBoundLab
{
    std::vector<size_t> offsets;
    std::vector<double> costs;
    std::vector<uint16_t> nearest;
    size_t count;
    double scale = 0;

  public:
    template <class Cost>
    ContactBoundLab(std::span<const size_t> contacts, const Cost &read, const std::function<bool()> &canceled)
        : count(contacts.size())
    {
        if (count > 256)
            throw std::runtime_error("Contact bound supports at most 256 catalog nodes");
        offsets.push_back(0);
        for (size_t n : contacts)
            offsets.push_back(offsets.back() + n);
        costs.assign(offsets.back() * count, std::numeric_limits<double>::infinity());
        nearest.resize(costs.size());
        for (size_t a = 0; a < count; a++)
        {
            if (canceled())
                throw std::runtime_error("Canceled");
            for (size_t x = 0; x < contacts[a]; x++)
            {
                size_t row = (offsets[a] + x) * count;
                for (size_t b = 0; b < count; b++)
                    if (a != b)
                        for (size_t y = 0; y < contacts[b]; y++)
                            costs[row + b] = std::min(costs[row + b], read(a, x, b, y));
                for (size_t b = 0; b < count; b++)
                    if (std::isfinite(costs[row + b]))
                        scale = std::max(scale, std::abs(costs[row + b]));
                std::iota(nearest.begin() + row, nearest.begin() + row + count, uint16_t{0});
                std::sort(nearest.begin() + row, nearest.begin() + row + count,
                          [&](int i, int j) { return costs[row + i] < costs[row + j]; });
            }
        }
    }

    double Lower(std::span<const int> nodes, const std::function<bool()> &canceled) const
    {
        if (nodes.size() < 2)
            return 0;
        std::array<uint64_t, 4> selected{};
        for (int node : nodes)
            selected[node / 64] |= uint64_t{1} << (node % 64);
        double sum = 0, endpoint = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < nodes.size(); i++)
        {
            if (canceled())
                throw std::runtime_error("Canceled");
            int node = nodes[i];
            double one = std::numeric_limits<double>::infinity(), two = one;
            for (size_t contact = offsets[node]; contact < offsets[node + 1]; contact++)
            {
                size_t row = contact * count;
                double first = std::numeric_limits<double>::infinity();
                for (size_t rank = 0; rank < count; rank++)
                {
                    int other = nearest[row + rank];
                    if (other == node || !(selected[other / 64] & (uint64_t{1} << (other % 64))))
                        continue;
                    if (!std::isfinite(first))
                        first = costs[row + other];
                    else
                    {
                        two = std::min(two, first + costs[row + other]);
                        break;
                    }
                }
                one = std::min(one, first);
            }
            if (i == 0)
                sum = one;
            else if (nodes.size() == 2)
                return std::isfinite(one)
                           ? std::max(0., one * (1 - 64 * std::numeric_limits<double>::epsilon()))
                           : 0;
            else
            {
                sum += two;
                endpoint = std::min(endpoint, one - two);
            }
        }
        double value = .5 * (sum + endpoint);
        // Conservative rounding allowance, including cancellation in the
        // unknown endpoint correction. Nonfinite relaxation safely does no pruning.
        double error = std::numeric_limits<double>::epsilon() * (std::abs(sum) + std::abs(endpoint)) *
                       (8 * nodes.size() + 32);
        return std::isfinite(value) && std::isfinite(error) ? std::max(0., value - error) : 0;
    }

    size_t Bytes() const
    {
        return offsets.size() * sizeof(size_t) + costs.size() * sizeof(double) +
               nearest.size() * sizeof(uint16_t);
    }

    // Lagrangian balance of the local incoming/outgoing choices. For arbitrary
    // vertex penalties p, subtract the known open-path degree contribution.
    // The endpoint correction includes +2*p[e] before taking its minimum.
    double Tighten(std::span<const int> nodes, double lower, double cutoff,
                   const std::function<bool()> &canceled, int rounds = 8) const
    {
        if (nodes.size() < 3)
            return lower;
        std::array<double, 256> penalty{}, one{}, two{}, gradient{};
        std::array<int, 256> single{}, left{}, right{};
        for (int round = 0; round < rounds; round++)
        {
            double sum = 0, correction = std::numeric_limits<double>::infinity();
            double penaltySum = 0, magnitude = 0;
            int endpoint = -1;
            for (size_t i = 0; i < nodes.size(); i++)
            {
                if (canceled())
                    throw std::runtime_error("Canceled");
                int v = nodes[i];
                one[v] = two[v] = std::numeric_limits<double>::infinity();
                single[v] = left[v] = right[v] = -1;
                for (size_t contact = offsets[v]; contact < offsets[v + 1]; contact++)
                {
                    double a = std::numeric_limits<double>::infinity(), b = a;
                    int ai = -1, bi = -1;
                    const auto *row = costs.data() + contact * count;
                    for (int w : nodes)
                        if (w != v)
                        {
                            double cost = row[w] + penalty[w];
                            if (cost < a)
                            {
                                b = a;
                                bi = ai;
                                a = cost;
                                ai = w;
                            }
                            else if (cost < b)
                            {
                                b = cost;
                                bi = w;
                            }
                        }
                    if (a + penalty[v] < one[v])
                    {
                        one[v] = a + penalty[v];
                        single[v] = ai;
                    }
                    if (a + b + 2 * penalty[v] < two[v])
                    {
                        two[v] = a + b + 2 * penalty[v];
                        left[v] = ai;
                        right[v] = bi;
                    }
                }
                magnitude = std::max(magnitude, std::abs(penalty[v]));
                if (i == 0)
                    sum = one[v];
                else
                {
                    sum += two[v];
                    penaltySum += penalty[v];
                    double delta = one[v] - two[v] + 2 * penalty[v];
                    if (delta < correction)
                    {
                        correction = delta;
                        endpoint = v;
                    }
                }
            }
            double value = .5 * (sum + correction) - penalty[nodes[0]] - 2 * penaltySum;
            double error = 256 * nodes.size() * nodes.size() * std::numeric_limits<double>::epsilon() *
                           (scale + 2 * magnitude);
            if (!std::isfinite(value) || !std::isfinite(error) || endpoint < 0)
                return lower;
            lower = std::max(lower, value - error);
            if (lower > cutoff + 1e-6)
                return lower;
            gradient.fill(0);
            for (size_t i = 0; i < nodes.size(); i++)
            {
                int v = nodes[i];
                if (i == 0 || v == endpoint)
                {
                    gradient[v] -= .5;
                    gradient[single[v]] += .5;
                }
                else
                {
                    gradient[v] -= 1;
                    gradient[left[v]] += .5;
                    gradient[right[v]] += .5;
                }
            }
            double norm = 0;
            for (int v : nodes)
                norm += gradient[v] * gradient[v];
            if (!norm)
                return lower;
            double step = 1.5 * (cutoff - value + 1) / norm;
            if (!std::isfinite(step))
                return lower;
            for (int v : nodes)
                penalty[v] += step * gradient[v];
        }
        return lower;
    }
};
} // namespace practice
