#ifndef MODEL_HH
#define MODEL_HH

#include "dl-refcounted.hh"
#include "dl-dvi-file.hh"
#include "observer.hh"
#include <list>

enum ModelState {
    HAS_FILE,
    NO_FILE,
    ERROR
};

using DviLib::FileLoader;
using DviLib::DviFile;
using DviLib::RefCounted;
using std::string;

class Model : public RefCounted {
    ModelState state;
    DviFile *dvi_file;
    string file_name;
    string err_msg;
    vector <Observer *> observers;
    
public:
    Model (string file_name);
    Model (void);
    void add_observer (Observer& o)
    {
	observers.push_back (&o);
    }
    void notify (void)
    {
	typedef vector <Observer *>::const_iterator It;
	for (It i = observers.begin(); i != observers.end(); ++i)
	    (*i)->notify ();
    }
    ModelState get_state (void) { return state; }
    string get_error (void) {
	if (state == ERROR)
	    return err_msg;
	else
	    return "";
    };
    DviFile *get_dvi_file (void) {
	if (state == HAS_FILE)
	    return dvi_file;
	else
	    return 0;
    };
    string get_file_name (void) {
	if (state == HAS_FILE)
	    return file_name;
	else
	    return "";
    };
};

#endif
