#ifndef OBSERVER_HH
#define OBSERVER_HH

#include <dl-refcounted.hh>

using DviLib::RefCounted;

class Observer : public RefCounted {
public:
    virtual void notify (void) const = 0;
    virtual ~Observer() {}
};


#endif /* OBSERVER_HH */
