#include "dl-loader.hh"
#include <errno.h>
#include <iostream>

using namespace DviLib;

/* Abstract loader */
/* =============== */

/* unsigned integers */
int 
AbstractLoader::get_uint16 ()
{
    return (get_uint8() << 8) | get_uint8();
}

int 
AbstractLoader::get_uint24 ()
{
    return (get_uint16() << 8) | get_uint8();
}
int 
AbstractLoader::get_uint32 ()
{
    return (get_uint16() << 16) | get_uint16();
}

/* signed integers */
int 
AbstractLoader::get_int16 ()
{
    return (get_int8() << 8) | get_uint8();
}

int 
AbstractLoader::get_int24 ()
{
    return (get_int16() << 8) | get_uint8();
}

int 
AbstractLoader::get_int32 ()
{
    return (get_int16() << 16) | get_uint16();
}

/* (Pascal) strings */
string 
AbstractLoader::get_string8 ()
{
    return get_n (get_uint8());
}

string 
AbstractLoader::get_string16 ()
{
    return get_n (get_uint16());
}

string
AbstractLoader::get_string24 ()
{
    return get_n (get_uint24());
}

string 
AbstractLoader::get_string32 ()
{
    return get_n (get_uint32());
}

void 
AbstractLoader::skip_string8 ()
{
    get_string8();
}

void 
AbstractLoader::skip_string16 ()
{
    get_string16();
}

void 
AbstractLoader::skip_string24 ()
{
    get_string24();
}

void 
AbstractLoader::skip_string32 ()
{
    get_string32();
}


/* "n" */
void 
AbstractLoader::skip_n (int n)
{
    get_n(n);
}

string 
AbstractLoader::get_n (int n)
{
    string r;
    
    while (n--)
	r += get_uint8 ();
    
    return r;
}

void
AbstractLoader::get_n (int n, unsigned char *v)
{
    while (n--)
	*v++ = (unsigned char)get_uint8 ();
}

/* File loader */

/* FIXME
 * 
 * do not use C style files (?)
 * what exceptions should we throw?
 */

FileLoader::FileLoader (const string &name)
{
    filename = name;
    f = fopen (filename.c_str(), "r");
    if (!f)
    {
	string s (strerror (errno));
	throw string ("Could not open " + filename + ": " + s);
    }
}

FileLoader::~FileLoader ()
{
    std::cout << "hej" << std::endl;
    if (fclose (f) == EOF)
	throw string ("Error closing " + filename);
}

int
FileLoader::get_int8 ()
{
    int c; 
    
    if ((c = fgetc (f)) == EOF)
	throw string ("Unexpected end of file");
    else
	return (signed char)c;
}

int
FileLoader::get_uint8 ()
{
    return (unsigned char)get_int8();
}

void
FileLoader::goto_from_start (int n)
{
    if (fseek (f, n, SEEK_SET) < 0)
    {
	string error = "fseek failed: ";
	error += strerror (errno);
	throw error;
    }
}

void
FileLoader::goto_from_current (int n)
{
    if (fseek (f, n, SEEK_CUR) < 0)
    {
	string error = "fseek failed: ";
	error += strerror (errno);
	throw error;
    }
}

void
FileLoader::goto_from_end (int n)
{
    if (fseek (f, n, SEEK_END) < 0)
    {
	string error = "fseek failed: ";
	error += strerror (errno);
	throw error;
    }    
}
