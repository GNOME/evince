#include "model.hh"

Model::Model (string file_name) 
{
    try {
	FileLoader *fl = new FileLoader (file_name);
	dvi_file = new DviFile (*fl);
	state = HAS_FILE;
    }
    catch (string e) {
	dvi_file = 0;
	state = ERROR;
	err_msg = e;
	cout << "error" << endl;
    }
}

Model::Model (void) 
{
    state = NO_FILE;
    dvi_file = 0;
}
    
