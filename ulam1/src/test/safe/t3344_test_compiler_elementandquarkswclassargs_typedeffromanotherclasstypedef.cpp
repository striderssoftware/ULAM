#include "TestCase_EndToEndCompiler.h"

namespace MFM {

  BEGINTESTCASECOMPILER(t3344_test_compiler_elementandquarkswclassargs_typedeffromanotherclasstypedef)
  {
    std::string GetAnswerKey()
    {
      //Constants have explicit types
      return std::string("Exit status: 3\nUe_P { Bool(1) b(false);  Int(32) test() {  Unsigned(3) var;  var 3u cast = ( var )func var cast return } }\nUq_Q { constant Int(32) s = NONREADYCONST;  typedef Unsigned(3) Foo;  <NOMAIN> }\nUq_V { typedef Q(3) Woof;  <NOMAIN> }\n");
    }

    std::string PresetTest(FileManagerString * fms)
    {
      //informed by t3339: recursive typedefs as function parameter
      // must already be parsed!
      bool rtn1 = fms->add("P.ulam","ulam 1;\nuse Q;\nuse V;\n element P {\nVoid func(V.Woof.Foo arg) {\n return ;\n}\n Bool b;\nInt test() {\n V.Woof.Foo var = 3u;\n func(var);\n return var;\n}\n}\n");

      bool rtn2 = fms->add("Q.ulam","ulam 1;\nquark Q(Int s) {\ntypedef Unsigned(s) Foo;\n}\n");

      bool rtn3 = fms->add("V.ulam","ulam 1;\nuse Q;\n quark V {\ntypedef Q(3) Woof;\n}\n");

      if(rtn1 && rtn2 && rtn3)
	return std::string("P.ulam");

      return std::string("");
    }
  }

  ENDTESTCASECOMPILER(t3344_test_compiler_elementandquarkswclassargs_typedeffromanotherclasstypedef)

} //end MFM
