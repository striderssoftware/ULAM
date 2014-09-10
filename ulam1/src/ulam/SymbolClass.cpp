#include <sstream>
#include <string.h>
#include "CompilerState.h"
#include "SymbolClass.h"

namespace MFM {

  SymbolClass::SymbolClass(u32 id, UTI utype, NodeBlockClass * classblock, CompilerState& state) : Symbol(id, utype, state), m_classBlock(classblock){}

  SymbolClass::~SymbolClass()
  {
    delete m_classBlock;
    m_classBlock = NULL;
  }


  void SymbolClass::setClassBlockNode(NodeBlockClass * node)
  {
    m_classBlock = node;
  }


  NodeBlockClass * SymbolClass::getClassBlockNode()
  {
    return m_classBlock;
  }
    

  bool SymbolClass::isClass()
  {
    return true;
  }


  const std::string SymbolClass::getMangledPrefix()
  {
    //    return ((UlamTypeClass *) getUlamType())->getUlamTypeAsStringForC();
    return m_state.getUlamTypeByIndex(getUlamTypeIdx())->getUlamTypeUPrefix();
  }


  ULAMCLASSTYPE SymbolClass::getUlamClass()
  {
    return  m_state.getUlamTypeByIndex(getUlamTypeIdx())->getUlamClass();
  }


  void SymbolClass::setUlamClass(ULAMCLASSTYPE type)
  {
    ((UlamTypeClass * ) m_state.getUlamTypeByIndex(getUlamTypeIdx()))->setUlamClass(type);
  }


} //end MFM
