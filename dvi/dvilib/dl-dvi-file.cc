#include "dl-dvi-file.hh"
#include "dl-dvi-parser.hh"

using namespace DviLib;

DviFile::DviFile (AbstractLoader& l) :
    loader (l)
{
    DviParser parser (loader);
    
    preamble = parser.parse_preamble ();
    postamble = parser.parse_postamble ();
    
    n_pages = 0;
    uint page_pointer = postamble->last_page_address;

    cout << page_pointer << endl;

    while (page_pointer != (uint)-1)
    {
	loader.goto_from_start (page_pointer);
	
	page_headers[n_pages++] = 
	    parser.parse_page_header (&page_pointer);
    }
}

DviPage *
DviFile::get_page (uint n)
{
    DviPage *page = pages[n];
    
    if (n > get_n_pages())
	return 0;
    
    if (page == 0)
    {
	DviParser parser (loader);
	DviPageHeader *header;
	DviProgram *program;
	
	header = page_headers[n];
	loader.goto_from_start (header->address + 45);
	program = parser.parse_program ();
	
	page = new DviPage (*header, *program);
    }
    
    return page;
}
