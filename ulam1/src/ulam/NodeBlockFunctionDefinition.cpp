#include <stdio.h>
#include "NodeBlockFunctionDefinition.h"
#include "CompilerState.h"
#include "SymbolVariable.h"
#include "SymbolFunctionName.h"

namespace MFM {

  NodeBlockFunctionDefinition::NodeBlockFunctionDefinition(SymbolFunction * fsym, NodeBlock * prevBlockNode, CompilerState & state, NodeStatements * s) : NodeBlock(prevBlockNode, state, s), m_funcSymbol(fsym), m_isDefinition(false), m_maxDepth(0), m_native(false), m_fsymTemplate(NULL)
  {}

  NodeBlockFunctionDefinition::NodeBlockFunctionDefinition(const NodeBlockFunctionDefinition& ref) : NodeBlock(ref), m_funcSymbol(NULL) /* shallow */, m_isDefinition(ref.m_isDefinition), m_maxDepth(ref.m_maxDepth), m_native(ref.m_native), m_fsymTemplate(ref.m_funcSymbol) {}

  NodeBlockFunctionDefinition::~NodeBlockFunctionDefinition()
  {
    // nodes deleted by SymbolTable in BlockClass
  }

  Node * NodeBlockFunctionDefinition::instantiate()
  {
    return new NodeBlockFunctionDefinition(*this);
  }

  void NodeBlockFunctionDefinition::print(File * fp)
  {
    printNodeLocation(fp);
    UTI myut = getNodeType();
    char id[255];
    if(myut==Nav)
      sprintf(id,"%s<NOTYPE>\n", prettyNodeName().c_str());
    else
      sprintf(id,"%s<%s>\n", prettyNodeName().c_str(), m_state.getUlamTypeNameByIndex(myut).c_str());
    fp->write(id);

    //parameters in symbol

    // if just a declaration, m_nextNode is NULL
    if(isDefinition())
      m_nextNode->print(fp);

    sprintf(id,"maxdepth=%d ----------------%s\n", m_maxDepth, prettyNodeName().c_str());
    fp->write(id);
  } //print

  void NodeBlockFunctionDefinition::printPostfix(File * fp)
  {
    fp->write(" ");
    fp->write(m_state.getUlamTypeNameBriefByIndex(m_funcSymbol->getUlamTypeIdx()).c_str()); //short type name
    fp->write(" ");
    fp->write(getName());
    // has no m_node!
    // declaration has no m_nextNode!!
    fp->write("(");
    u32 numparams = m_funcSymbol->getNumberOfParameters();

    for(u32 i = 0; i < numparams; i++)
      {
	if(i > 0)
	  fp->write(", ");

	Symbol * asym = m_funcSymbol->getParameterSymbolPtr(i);
	assert(asym);
	fp->write(m_state.getUlamTypeNameBriefByIndex(asym->getUlamTypeIdx()).c_str()); //short type name
	fp->write(" ");
	fp->write(m_state.m_pool.getDataAsString(asym->getId()).c_str());

	s32 arraysize = 0;
	if(asym->isDataMember() && !asym->isFunction())
	  {
	    arraysize = m_state.getArraySize( ((SymbolVariable *) asym)->getUlamTypeIdx());
	  }

	if(arraysize > NONARRAYSIZE)
	  {
	    fp->write("[");
	    fp->write_decimal(arraysize);
	    fp->write("]");
	  }
      }
    fp->write(")");

    if(isDefinition())
      {
	fp->write(" { ");
	m_nextNode->printPostfix(fp);
	fp->write(" }");
      }
    else
      fp->write(";");
  } //printPostfix

  const char * NodeBlockFunctionDefinition::getName()
  {
    return m_state.m_pool.getDataAsString(m_funcSymbol->getId()).c_str();
  }

  const std::string NodeBlockFunctionDefinition::prettyNodeName()
  {
    return nodeName(__PRETTY_FUNCTION__);
  }

  UTI NodeBlockFunctionDefinition::checkAndLabelType()
  {
    // instantiate, look up in class block
    if(m_funcSymbol == NULL && m_fsymTemplate != NULL)
      {
	Symbol * asymptr = NULL;
	if(m_state.alreadyDefinedSymbol(m_fsymTemplate->getId(), asymptr))
	  {
	    if(asymptr->isFunction())
	      {
		// still need the SymbolFunction, this is the SymbolFunctionName
		// build parameter types list from saved template FunctionSymbol
		std::vector<UTI> m_paramTypes;
		for(u32 i = 0; i < m_fsymTemplate->getNumberOfParameters(); i++)
		  {
		    Symbol * psym = m_fsymTemplate->getParameterSymbolPtr(i);
		    m_paramTypes.push_back(psym->getUlamTypeIdx());
		  }
		SymbolFunction * fsymclone = NULL;
		if(((SymbolFunctionName *) asymptr)->findMatchingFunction(m_paramTypes, fsymclone))
		  m_funcSymbol = (SymbolFunction *) fsymclone;
	      }
	    else
	      {
		std::ostringstream msg;
		msg << "(1) <" << m_state.m_pool.getDataAsString(m_fsymTemplate->getId()).c_str() << "> is not a function, and cannot be used as one";
		MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	      }
	  }
	else
	  {
	    std::ostringstream msg;
	    msg << "(2) Function <" << m_state.m_pool.getDataAsString(m_fsymTemplate->getId()).c_str() << "> is not defined, and cannot be used";
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	  }
	if(!m_funcSymbol)
	  return Nav;
      } //toinstantiate

    UTI it = m_funcSymbol->getUlamTypeIdx();
    setNodeType(it);

    m_state.m_currentBlock = this;

    m_state.m_currentFunctionReturnNodes.clear(); //vector of return nodes
    m_state.m_currentFunctionReturnType = it;

    if(m_nextNode) //non-empty function
      {
	m_nextNode->checkAndLabelType();                     //side-effect
	m_state.checkFunctionReturnNodeTypes(m_funcSymbol);  //gives errors
      }
    else
      {
	std::ostringstream msg;
	msg << "Undefined function block: <" << getName() << ">";
	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
      }
    m_state.m_currentBlock = NodeBlock::getPreviousBlockPointer();  //missing?
    return getNodeType();
  } //checkAndLabelType

  EvalStatus NodeBlockFunctionDefinition::eval()
  {
    assert(isDefinition());
    assert(m_nextNode);

    // m_currentObjPtr set up by caller
    assert(m_state.m_currentObjPtr.getUlamValueTypeIdx() != Nav);
    m_state.m_currentFunctionReturnType = getNodeType(); //to help find hidden first arg

    evalNodeProlog(0);                  //new current frame pointer on node eval stack
    makeRoomForNodeType(getNodeType()); //place for return vals node eval stack

    m_state.m_funcCallStack.addFrameSlots(getMaxDepth());  //local variables on callstack!

    EvalStatus evs = m_nextNode->eval();

    PACKFIT packRtn = m_state.determinePackable(getNodeType());
    UlamValue rtnUV;

    if(evs == RETURN)
      {
	// save results in the stackframe for caller;
	// copies each element of the array by value,
	// in reverse order ([0] is last at bottom)
	s32 slot = m_state.slotsNeeded(getNodeType());
	rtnUV = UlamValue::makePtr(-slot, STACK, getNodeType(), packRtn, m_state); //negative to current stack frame pointer
      }
    else if (evs == NORMAL)  //no explicit return statement
      {
	// 1 for base of array or scalar
	rtnUV = UlamValue::makePtr(1, EVALRETURN, getNodeType(), packRtn, m_state); //positive to current frame pointer
      }
    else
      {
	m_state.m_funcCallStack.returnFrame();
	evalNodeEpilog();
	return evs;
      }

    //save results in the node eval stackframe for function caller
    //return each element of the array by value, in reverse order ([0] is last at bottom)
    assignReturnValueToStack(rtnUV);

    m_state.m_funcCallStack.returnFrame();
    evalNodeEpilog();
    return NORMAL;
  }

  void NodeBlockFunctionDefinition::setDefinition()
  {
    m_isDefinition = true;
  }

  bool NodeBlockFunctionDefinition::isDefinition()
  {
    return m_isDefinition;
  }

  void NodeBlockFunctionDefinition::setMaxDepth(u32 depth)
  {
    m_maxDepth = depth;

#if 0
    std::ostringstream msg;
    msg << "Max Depth is: <" << m_maxDepth << ">";
    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), INFO);
#endif
  }

  u32 NodeBlockFunctionDefinition::getMaxDepth()
  {
    return m_maxDepth;
  }

  void NodeBlockFunctionDefinition::setNative()
  {
    m_native = true;
  }

  bool NodeBlockFunctionDefinition::isNative()
  {
    return m_native;
  }

  SymbolFunction * NodeBlockFunctionDefinition::getFuncSymbolPtr()
  {
    return m_funcSymbol;
  }

  void NodeBlockFunctionDefinition::genCode(File * fp, UlamValue& uvpass)
  {
    // m_currentObjSymbol set up by caller
    //    assert(m_state.m_currentObjSymbolForCodeGen != NULL);
    m_state.m_currentBlock = this;

    assert(isDefinition());
    assert(m_nextNode);

    assert(!isNative());

    fp->write("\n");
    m_state.indent(fp);
    fp->write("{\n");

    m_state.m_currentIndentLevel++;

    m_nextNode->genCode(fp, uvpass);

    m_state.m_currentIndentLevel--;

    fp->write("\n");
    m_state.indent(fp);
    fp->write("} // ");
    fp->write(m_funcSymbol->getMangledName().c_str());  //end of function
    fp->write("\n\n\n");

    m_state.m_currentBlock = NodeBlock::getPreviousBlockPointer();  //missing?
  } //genCode

} //end MFM
