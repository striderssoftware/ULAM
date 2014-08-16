#include <iostream>
#include <stdio.h>
#include "TestCase_EndToEndCompiler.h"
#include "Compiler.h"

namespace MFM {

  bool TestCase_EndToEndCompiler::GetTestResults(FileManager * fm, std::string startstr, File * output)
  {
    Compiler C;
    bool rtn = false;

    // error messages appended to output are compared to answer
    //Node * programme = C.start(fm,startstr,output);
    Node * programme = NULL;
    if(C.parseProgram(fm,startstr, output, programme) == 0)
      {
	rtn = true;
	if(C.checkAndTypeLabelProgram(programme, output) == 0)
	  {
	    s32 exitReturnValue;	    
	    
	    if(C.testProgram(programme, output, exitReturnValue) == 0)
	      //C.printPostFix(programme, output);
	      C.generateCodedProgram(programme, output); //temp!!! XXX
	    else
	      output->write("Unrecoverable Program Test FAILURE.\n");

	    output->write("Exit status: " );    //in compared answer
	    output->write_decimal(exitReturnValue);	      
	  }
	else
	  output->write("Unrecoverable Program Type Label FAILURE.\n");
      }
    else
      {	
	output->write("Unrecoverable Program Parse FAILURE.\n");
      }
    
    delete programme;
    return rtn;
  }

  

  bool TestCase_EndToEndCompiler::CheckResults(FileManagerString * fms, File * errorOutput)
  {    
    return CompareResultsWithAnswerKey(fms,errorOutput);
  }
      
} //end MFM
