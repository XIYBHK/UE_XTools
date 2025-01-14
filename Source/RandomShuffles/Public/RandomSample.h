/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
#pragma once

#include "MinIndexQueue.h"
#include <cmath>
#include <iterator>

namespace RandomShuffles {

template<typename It, typename Wt, typename Out, typename Rand>
Out RandomSample(It begin, It end, Wt wbegin, Out out, std::size_t count, Rand randFunc) {
    using std::distance;

    auto sampleSize = static_cast<std::size_t>(std::distance(begin, end));
    if (sampleSize < count) {
        count = sampleSize;
    }
    if (count == 0) {
        return out;
    }

    MinIndexQueue H(sampleSize);
	auto i = begin;

    // 将至少所需数量推入优先队列
    while(H.Size() < count) {
        float W = static_cast<float>(*wbegin++);
        float R = 0.0f;
        if (W > 0.0f) {
            float U = randFunc(0.0f, 1.0f);
            R = std::pow(U, 1.0f/W);
        }
        H.Push(R, distance(begin, i++));
    }

    // 从数组中随机选择更多
    // Randomly select some more from the array
    float Jump = std::logf(randFunc(0.0f, 1.0f)) / std::logf(H.MinimumKey());

    while (i != end) {
        float W = static_cast<float>(*wbegin++);
        Jump -= W;

        if (Jump <= 0) {
            float T = std::pow(H.MinimumKey(), W);
			float R = 0.0f;
			if (W > 0.0f) {
				float U = randFunc(T, 1.0);
				R = std::pow(U, 1.0f/W);
			}

            H.ExtractMin();
            H.Push(R, distance(begin, i));
            Jump = std::logf(randFunc(0.0f, 1.0f)) / std::logf(H.MinimumKey());
        }
		++i;
    }

    // 从队列中选择前几个到输出数组
    for (std::size_t J = 0; J < count; J++) {
        std::size_t Index = H.ExtractMin();
		*out++ = *std::next(begin, Index);
    }
    return out;
}
}