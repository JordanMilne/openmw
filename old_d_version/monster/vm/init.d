/*
  Monster - an advanced game scripting language
  Copyright (C) 2007-2009  Nicolay Korslund
  Email: <korslund@gmail.com>
  WWW: http://monster.snaptoad.com/

  This file (init.d) is part of the Monster script language package.

  Monster is distributed as free software: you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 3, as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  version 3 along with this program. If not, see
  http://www.gnu.org/licenses/ .

 */

// This module makes sure that the library is initialized in all
// cases.
module monster.vm.init;

import monster.compiler.tokenizer;
import monster.compiler.properties;
import monster.compiler.scopes;
import monster.vm.thread;
import monster.vm.stack;
import monster.vm.mclass;
import monster.vm.arrays;
import monster.vm.vm;
import monster.vm.dbg;

import monster.modules.all;
import monster.options;

version(Tango)
{}
else
{

// D runtime stuff
version(Posix)
{
  extern (C) void _STI_monitor_staticctor();
  //extern (C) void _STD_monitor_staticdtor();
  extern (C) void _STI_critical_init();
  //extern (C) void _STD_critical_term();
}
version(Win32)
{
  extern (C) void _minit();
}
extern (C) void gc_init();
//extern (C) void gc_term();
extern (C) void _moduleCtor();
//extern (C) void _moduleDtor();
extern (C) void _moduleUnitTests();

//extern (C) bool no_catch_exceptions;

} // end version(Tango) .. else


bool initHasRun = false;

bool stHasRun = false;

static this()
{
  assert(!stHasRun);
  stHasRun = true;

  // While we're here, run the initializer right away if it hasn't run
  // already.
  if(!initHasRun)
    doMonsterInit();
}

void doMonsterInit()
{
  // Prevent recursion
  assert(!initHasRun, "doMonsterInit should never run more than once");
  initHasRun = true;

  // First check if D has been initialized.
  if(!stHasRun)
    {
      // Nope. This is normal though if we're running as a C++
      // library. We have to init the D runtime manually.

      // But this is not supported in Tango at the moment.
      version(Tango)
        {
          assert(0, "tango-compiled C++ library not supported yet");
        }
      else
        {

          version (Posix)
            {
              _STI_monitor_staticctor();
              _STI_critical_init();
            }

          gc_init();

          version (Win32)
            {
              _minit();
            }

          _moduleCtor();
          _moduleUnitTests();
        }
    }

  assert(stHasRun, "D library initializion failed");

  // Next, initialize the Monster library

  // Initialize the debugger structure
  dbg.init();

  // Initialize tokenizer
  initTokenizer();

  // initScope depends on doVMInit setting vm.vfs
  vm.doVMInit();
  initScope();

  // The rest of the VM
  scheduler.init();
  stack.init();
  arrays.initialize();

  // Compiles the 'Object' class
  MonsterClass.initialize();

  // Depends on 'Object'
  initProperties();

  // Load modules
  static if(loadModules)
    initAllModules();
}