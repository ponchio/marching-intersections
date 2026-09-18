#ifndef FILTER_H
#define FILTER_H

#include "intersections.h"

namespace mi {

class Filter {
public:
    virtual ~Filter() {}
    virtual void apply(Volume &vol, const Box3i &box) = 0;
};

class CarveSphereFilter : public Filter {
public:
    CarveSphereFilter() {}
    void apply(Volume &vol, const Box3i &box) override;
};

void applyCarveSphere(Volume &vol, const Box3i &box);

} // namespace mi

#endif // FILTER_H
