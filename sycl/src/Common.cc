#include "Common.h"
#include <algorithm>

void
MultiplyVectorByScalar(std::vector<float> &v, float k) {
    std::transform(v.begin(), v.end(), v.begin(), [k](float &c) {
        return c * k;
    });
}