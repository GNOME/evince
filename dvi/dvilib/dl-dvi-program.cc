#include "dl-dvi-program.hh"
#include <algorithm>

using namespace DviLib;

typedef vector<DviCommand *>::iterator It;

void
DviProgram::execute (DviRuntime& runtime)
{
    for (It i = commands.begin(); i != commands.end(); ++i)	
    {
	(*i)->execute (runtime);
    }
}

void 
DviProgram::add_command (DviCommand *cmd)
{
    cmd->ref();
    commands.push_back (cmd);
}

DviProgram::~DviProgram (void)
{
    for (It i = commands.begin(); i != commands.end(); ++i)
    {
	(*i)->unref ();
    }
}
