#ifndef DL_DVI_PROGRAM_HH__
#define DL_DVI_PROGRAM_HH__

using namespace std;

#include <string>
#include <vector>

#include <iostream>

#include "dl-refcounted.hh"
#include "dl-dvi-runtime.hh"

namespace DviLib
{
    class DviCommand : public RefCounted
    {
    public:
	virtual void execute (DviRuntime& runtime) = 0;
	virtual ~DviCommand() {};
    };
    
    class AbstractDviProgram : public RefCounted
    {
    public:
	virtual void execute (DviRuntime &runtime) = 0;
	virtual ~AbstractDviProgram (void) {};
    };
    
    class DviProgram : public AbstractDviProgram
    {
    public:
	vector <DviCommand *> commands;
	void add_command (DviCommand *cmd);
	virtual void execute (DviRuntime& runtime);
	virtual ~DviProgram (void);
    };
    
    class DviCharCommand : public DviCommand
    {
    private:
	uint c;
	
    public:
	DviCharCommand (uint c_arg)
	{
	    c = c_arg;
	}
	uint get_c (void) const { return c; }
    };
    
    class DviPutCharCommand : public DviCharCommand
    {
    public:
	DviPutCharCommand (uint ch) : DviCharCommand (ch) {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.put_char (get_c());
	}
    };
    
    class DviSetCharCommand : public DviCharCommand
    {
    public:
	DviSetCharCommand (uint ch) : DviCharCommand (ch) {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.set_char (get_c());
	}
    };
    
    class DviRuleCommand : public DviCommand
    {
    private:
	int h, w;
	
    public:
	DviRuleCommand (int h_arg, int w_arg) : h(h_arg), w(w_arg) 
	{
	    std::cout << "rule cmd " << h << " " << w << std::endl;
	}
	int get_h (void) const { return h; }
	int get_w (void) const { return w; }
    };
    
    class DviPutRuleCommand : public DviRuleCommand
    {
    public:
	DviPutRuleCommand (int h, int w) : DviRuleCommand (h, w) {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.put_rule (get_h(), get_w());
	}
    };
    
    class DviSetRuleCommand : public DviRuleCommand
    {
    public:
	DviSetRuleCommand (int h, int w) : DviRuleCommand (h, w) {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.set_rule (get_h(), get_w());
	}
    };
    
    class DviPushCommand : public DviCommand
    {
    public:
	DviPushCommand () {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.push ();
	}
    };
    
    class DviPopCommand : public DviCommand
    {
    public:
	DviPopCommand () {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.pop ();
	}
    };
    
    class DviMoveCommand : public DviCommand
    {
    private:
	int len;
	
    public:
	DviMoveCommand (int len_arg) : len (len_arg) {};
	int get_len (void) { return len; }
    };
    
    class DviRightCommand : public DviMoveCommand
    {
    public:
	DviRightCommand (int len) : DviMoveCommand (len) 
	{
#if 0
	    cout << "right command " << get_len() << endl;
#endif
	};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.right (get_len());
	}
    };
    
    class DviWCommand : public DviMoveCommand
    {
    public:
	DviWCommand (int len) : DviMoveCommand (len) {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.w (get_len());
	}
    };
    
    class DviXCommand : public DviMoveCommand
    {
    public:
	DviXCommand (int len) : DviMoveCommand (len) {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.x (get_len());
	}
    };
    
    class DviDownCommand : public DviMoveCommand
    {
    public:
	DviDownCommand (int len) : DviMoveCommand (len) 
	{
#if 0
	    cout << "down command " << get_len() << endl;
#endif
	};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.down (get_len());
	}
    };
    
    class DviYCommand : public DviMoveCommand
    {
    public:
	DviYCommand (int len) : DviMoveCommand (len) {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.y (get_len());
	}
    };
    
    class DviZCommand : public DviMoveCommand
    {
    public:
	DviZCommand (int len) : DviMoveCommand (len) {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.z (get_len());
	}
    };
    
    class DviFontNumCommand : public DviCommand
    {
    private:
	int num;
	
    public:
	DviFontNumCommand (int num_arg) : num (num_arg) {}
	virtual void execute (DviRuntime& runtime)
	{
	    runtime.font_num (num);
	}
    };
    
    class DviSpecialCommand : public DviCommand
    {
	string spc;
    public:
	DviSpecialCommand (string s) : spc (s) {};
	virtual ~DviSpecialCommand () {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.special (spc);
	}
    };
    
    class DviWRepCommand : public DviCommand
    {
    public:
	DviWRepCommand () {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.w_rep ();
	}
    };
    
    class DviXRepCommand : public DviCommand
    {
    public:
	DviXRepCommand () {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.x_rep ();
	}
    };
    
    class DviYRepCommand : public DviCommand
    {
    public:
	DviYRepCommand () {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.y_rep ();
	}
    };
    
    class DviZRepCommand : public DviCommand
    {
    public:
	DviZRepCommand () {};
	virtual void execute (DviRuntime& runtime) 
	{
	    runtime.z_rep ();
	}
    };
    
}
#endif // DL_DVI_PROGRAM_HH__
