#ifndef DL_REFCOUNTED_HH
#define DL_REFCOUNTED_HH

using namespace std;

typedef unsigned int uint;
typedef unsigned char uchar;

namespace DviLib {
    
    class RefCounted
    {
	int refcount;

    public:

	RefCounted (void)
	{
	    refcount = 1;
	}

	RefCounted *ref (void)
	{
	    refcount++;
	    return this;
	}

	void unref (void)
	{
	    refcount--;
	    if (!refcount)
		delete this;
	}
    };
}

#endif // DL_REFCOUNTED_HH
