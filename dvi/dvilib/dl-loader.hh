#ifndef DL_LOADER_HH
#define DL_LOADER_HH

#include <string>
#include <cstdio>
#include <vector>

#include "dl-refcounted.hh"

namespace DviLib {
    
    class AbstractLoader : public RefCounted {
    public:
	virtual int get_uint8 ()		= 0;
	virtual int get_uint16 ();
	virtual int get_uint24 ();
	virtual int get_uint32 ();
	
	virtual int get_int8 ()			= 0;
	virtual int get_int16 ();
	virtual int get_int24 ();
	virtual int get_int32 ();
	
	virtual string get_string8 ();
	virtual string get_string16 ();
	virtual string get_string24 ();
	virtual string get_string32 ();
	
	virtual void skip_string8 ();
	virtual void skip_string16 ();
	virtual void skip_string24 ();
	virtual void skip_string32 ();
	
	virtual void goto_from_start (int i)	= 0;
	virtual void goto_from_end (int i)	= 0;
	virtual void goto_from_current (int i)	= 0;
	
	virtual void skip_n (int n);
	virtual string get_n (int n);
	virtual void get_n (int n, unsigned char *v);
	
	virtual ~AbstractLoader() {};
    };
    
    class FileLoader : public AbstractLoader {
	FILE *f;
	string filename;
    public:
	FileLoader (const string &name);
	virtual int get_int8 ();
	virtual int get_uint8 ();
	virtual void goto_from_start (int i);
	virtual void goto_from_end (int i);
	virtual void goto_from_current (int i);
	
	virtual ~FileLoader ();
    };
}
#endif // DL_LOADER_HH
