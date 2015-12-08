#include <ctype.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include "UlamTypeClass.h"
#include "UlamValue.h"
#include "CompilerState.h"
#include "Util.h"

namespace MFM {

  UlamTypeClass::UlamTypeClass(const UlamKeyTypeSignature key, CompilerState & state, ULAMCLASSTYPE type) : UlamType(key, state), m_class(type), m_customArray(false) /* , m_customArrayType(Nav)*/
  {
    m_wordLengthTotal = calcWordSize(getTotalBitSize());
    m_wordLengthItem = calcWordSize(getBitSize());
  }

   ULAMTYPE UlamTypeClass::getUlamTypeEnum()
   {
     return Class;
   }

  bool UlamTypeClass::isNumericType()
  {
    if(m_class == UC_QUARK)
      {
	u32 quti = m_key.getUlamKeyTypeSignatureClassInstanceIdx();
	return (m_state.quarkHasAToIntMethod(quti));
      }
    return false;
  } //isNumericType

  bool UlamTypeClass::isPrimitiveType()
  {
    return isNumericType();
  }

  bool UlamTypeClass::cast(UlamValue & val, UTI typidx)
  {
    bool brtn = true;
    assert(m_state.getUlamTypeByIndex(typidx) == this); //tobe
    UTI valtypidx = val.getUlamValueTypeIdx();
    UlamType * vut = m_state.getUlamTypeByIndex(valtypidx);
    assert(vut->isScalar() && isScalar());

    //now allowing atoms to be cast as quarks, as well as elements;
    // also allowing subclasses to be cast as their superclass (u1.2.2)
    if(m_class == UC_ELEMENT)
      {
	if(!(valtypidx == UAtom || UlamType::compare(valtypidx, typidx, m_state) == UTIC_SAME))
	  {
	    //no longer elements inherit from elements, only quarks.
	    brtn = false;
	  }
	else if(valtypidx == UAtom)
	  val.setAtomElementTypeIdx(typidx); //for testing purposes, assume ok
	//else true
      }
    else if(m_class == UC_QUARK)
      {
	if(valtypidx == UAtom)
	  brtn = false; //cast atom to a quark?
	else if(UlamType::compare(valtypidx, typidx, m_state) == UTIC_SAME)
	  {
	    //if same type nothing to do; if atom, shows as element in eval-land.
	    //val.setAtomElementTypeIdx(typidx); //?
	  }
	else if(m_state.isClassASuperclassOf(valtypidx, typidx))
	  {
	    //2 quarks, or element (val) inherits from this quark
	    ULAMCLASSTYPE vclasstype = vut->getUlamClass();
	    if(vclasstype == UC_ELEMENT)
	      {
		s32 pos = 0; //ancestors start at first state bit pos
		s32 len = getTotalBitSize();
		assert(len != UNKNOWNSIZE);
		if(len <= MAXBITSPERINT)
		  {
		    u32 qdata = val.getDataFromAtom(pos + ATOMFIRSTSTATEBITPOS, len);
		    val = UlamValue::makeImmediateQuark(typidx, qdata, len);
		  }
		else if(len <= MAXBITSPERLONG)
		  {
		    assert(0); //quarks are max 32 bits
		    u64 qdata = val.getDataLongFromAtom(pos + ATOMFIRSTSTATEBITPOS, len);
		    val = UlamValue::makeImmediateLong(typidx, qdata, len);
		  }
		else
		  assert(0);
	      }
	    else
	      {
		// both left-justified immediate quarks
		// Coo c = (Coo) f.su; where su is a Soo : Coo
		s32 vlen = vut->getTotalBitSize();
		s32 len = getTotalBitSize();
		u32 vdata = val.getImmediateQuarkData(vlen); //not from element
		assert((vlen - len) >= 0); //sanity check
		u32 qdata = vdata >> (vlen - len); //stays left-justified
		val = UlamValue::makeImmediateQuark(typidx, qdata, len);
	      }
	  }
	else
	  brtn = false;
      } //end quark casting
    else
      assert(0); //neither element nor quark

    return brtn;
  } //end cast

  FORECAST UlamTypeClass::safeCast(UTI typidx)
  {
    FORECAST scr = UlamType::safeCast(typidx);
    if(scr != CAST_CLEAR)
      return scr;

    if(m_state.getUlamTypeByIndex(typidx) == this)
      return CAST_CLEAR; //same class, quark or element
    else
      {
	u32 cuti = m_key.getUlamKeyTypeSignatureClassInstanceIdx();
	if(m_state.isClassASuperclassOf(cuti, typidx))
	  return CAST_CLEAR;
      }

    return CAST_BAD; //e.g. (typidx == UAtom)
  } //safeCast

  const char * UlamTypeClass::getUlamTypeAsSingleLowercaseLetter()
  {
    switch(m_class)
      {
      case UC_ELEMENT:
	return "e";
      case UC_QUARK:
	return "q";
      case UC_UNSEEN:
      default:
	assert(0);
      };
    return UlamType::getUlamTypeEnumCodeChar(getUlamTypeEnum());
  } //getUlamTypeAsSingleLowercaseLetter()

  const std::string UlamTypeClass::getUlamTypeVDAsStringForC()
  {
    return "VD::BITS"; //for quark use bits
  }

  const std::string UlamTypeClass::getUlamTypeMangledType()
  {
    // e.g. parsing overloaded functions, may not be complete.
    std::ostringstream mangled;
    s32 bitsize = getBitSize();
    s32 arraysize = getArraySize();

    if(arraysize > 0)
      mangled << ToLeximitedNumber(arraysize);
    else
      mangled << 10;

    if(bitsize > 0)
      mangled << ToLeximitedNumber(bitsize);
    else
      mangled << 10;

    mangled << m_state.getDataAsStringMangled(m_key.getUlamKeyTypeSignatureNameId()).c_str();
    //without types and values of args!!
    return mangled.str();
  } //getUlamTypeMangledType

  const std::string UlamTypeClass::getUlamTypeMangledName()
  {
    std::ostringstream mangledclassname;
    mangledclassname << UlamType::getUlamTypeMangledName(); //includes Uprefix

    //appends '10', or numberOfParameters followed by each digi-encoded: mangled type and value
    u32 id = m_key.getUlamKeyTypeSignatureNameId();
    UTI cuti =  m_key.getUlamKeyTypeSignatureClassInstanceIdx();
    SymbolClassName * cnsym = (SymbolClassName *) m_state.m_programDefST.getSymbolPtr(id);
    mangledclassname << cnsym->formatAnInstancesArgValuesAsAString(cuti);
    return mangledclassname.str();
  } //getUlamTypeMangledName

  //quarks are right-justified in an atom space
  const std::string UlamTypeClass::getUlamTypeAsStringForC()
  {
    assert(m_class != UC_UNSEEN);
    if(m_class == UC_QUARK)
      {
	return UlamType::getUlamTypeAsStringForC();
      }
    return "T"; //for elements
  } //getUlamTypeAsStringForC()

  const std::string UlamTypeClass::getUlamTypeUPrefix()
  {
    if(getArraySize() > 0)
      return "Ut_";

    switch(m_class)
      {
      case UC_ELEMENT:
	return "Ue_";
      case UC_QUARK:
	return "Uq_";
      case UC_UNSEEN:
	return "U?_";
     default:
	assert(0);
      };
    return "xx";
  } //getUlamTypeUPrefix

  const std::string UlamTypeClass::getUlamTypeNameBrief()
  {
    std::ostringstream namestr;
    namestr << m_key.getUlamKeyTypeSignatureName(&m_state).c_str();

    u32 id = m_key.getUlamKeyTypeSignatureNameId();
    u32 cuti = m_key.getUlamKeyTypeSignatureClassInstanceIdx();
    SymbolClassName * cnsym = (SymbolClassName *) m_state.m_programDefST.getSymbolPtr(id);
    if(cnsym && cnsym->isClassTemplate())
      namestr << ((SymbolClassNameTemplate *) cnsym)->formatAnInstancesArgValuesAsCommaDelimitedString(cuti).c_str();
    return namestr.str();
  } //getUlamTypeNameBrief

  //see SymbolVariableDataMember printPostfix for recursive output
  void UlamTypeClass::getDataAsString(const u32 data, char * valstr, char prefix)
  {
    if(prefix == 'z')
      sprintf(valstr,"%d", data);
    else
      sprintf(valstr,"%c%d", prefix, data);
  }

  void UlamTypeClass::getDataLongAsString(const u64 data, char * valstr, char prefix)
  {
    if(prefix == 'z')
      sprintf(valstr,"%s", ToSignedDecimal(data).c_str());
    else
      sprintf(valstr,"%c%s", prefix, ToSignedDecimal(data).c_str());
  }

  ULAMCLASSTYPE UlamTypeClass::getUlamClass()
  {
    return m_class;
  }

  void UlamTypeClass::setUlamClass(ULAMCLASSTYPE type)
  {
    m_class = type;
    if(m_class == UC_ELEMENT)
      {
	setTotalWordSize(BITSPERATOM);
	setItemWordSize(BITSPERATOM);
      }
  } //setUlamClass

  bool UlamTypeClass::isScalar()
  {
    return (m_key.getUlamKeyTypeSignatureArraySize() == NONARRAYSIZE);
  }

  bool UlamTypeClass::isCustomArray()
  {
    return m_customArray; //canonical, ignores ancestors
  }

  void UlamTypeClass::setCustomArray()
  {
    m_customArray = true;
  }

  UTI UlamTypeClass::getCustomArrayType()
  {
    u32 cuti = m_key.getUlamKeyTypeSignatureClassInstanceIdx();
    return m_state.getAClassCustomArrayType(cuti);
  } //getCustomArrayType

  u32 UlamTypeClass::getCustomArrayIndexTypeFor(Node * rnode, UTI& idxuti, bool& hasHazyArgs)
  {
    u32 cuti = m_key.getUlamKeyTypeSignatureClassInstanceIdx();
    return m_state.getAClassCustomArrayIndexType(cuti, rnode, idxuti, hasHazyArgs);
  } //getCustomArrayIndexTypeFor

  s32 UlamTypeClass::getBitSize()
  {
    s32 bitsize = m_key.getUlamKeyTypeSignatureBitSize();
    return bitsize; //could be negative "unknown"; allow for empty quarks
  }

  bool UlamTypeClass::isHolder()
  {
    std::string name = m_key.getUlamKeyTypeSignatureName(&m_state);
    s32 c = name.at(0);
    return (!isalpha(c)); //id same as UTI number, out-of-band
  }

  bool UlamTypeClass::isComplete()
  {
    if(isHolder())
      return false;

    return UlamType::isComplete();
  }

  bool UlamTypeClass::isMinMaxAllowed()
  {
    return false;
  }

  PACKFIT UlamTypeClass::getPackable()
  {
    if(m_class == UC_ELEMENT)
      return UNPACKED; //was PACKED, now matches ATOM regardless of its bit size.
    return UlamType::getPackable(); //quarks depend their size
  }

  bool UlamTypeClass::needsImmediateType()
  {
    if(!isComplete())
      return false;

    bool rtnb = false;
    if(m_class == UC_QUARK || m_class == UC_ELEMENT)
      {
	rtnb = true;
	u32 id = m_key.getUlamKeyTypeSignatureNameId();
	u32 cuti = m_key.getUlamKeyTypeSignatureClassInstanceIdx();
	SymbolClassName * cnsym = (SymbolClassName *) m_state.m_programDefST.getSymbolPtr(id);
	if(cnsym->isClassTemplate() && ((SymbolClassNameTemplate *) cnsym)->isClassTemplate(cuti))
	  rtnb = false; //the "template" has no immediate, only instances
      }
    return rtnb;
  } //needsImmediateType

  const std::string UlamTypeClass::getUlamTypeImmediateMangledName()
  {
    if(needsImmediateType())
      {
	return UlamType::getUlamTypeImmediateMangledName();
      }
    return "T";
  } //getUlamTypeImmediateMangledName

  const std::string UlamTypeClass::getUlamTypeImmediateAutoMangledName()
  {
    assert(needsImmediateType());
    std::ostringstream  automn;
    automn << getUlamTypeImmediateMangledName().c_str();
    automn << "4auto" ;
    return automn.str();
  } //getUlamTypeImmediateAutoMangledName

  const std::string UlamTypeClass::getTmpStorageTypeAsString()
  {
    if(m_class == UC_QUARK)
      {
	return UlamType::getTmpStorageTypeAsString(); // entire, u32 or u64
      }
    else if(m_class == UC_ELEMENT)
      {
 	return "T";
      }
    else
      assert(0);
    return ""; //error!
  } //getTmpStorageTypeAsString

  const std::string UlamTypeClass::getArrayItemTmpStorageTypeAsString()
  {
    if(!isScalar())
      return UlamType::getTmpStorageTypeAsString();

    return m_state.getUlamTypeByIndex(getCustomArrayType())->getTmpStorageTypeAsString();
  } //getArrayItemTmpStorageTypeAsString

  const std::string UlamTypeClass::getImmediateStorageTypeAsString()
  {
    std::ostringstream ctype;
    ctype << getUlamTypeImmediateMangledName();

    if(m_class == UC_QUARK)
      ctype << "<EC>"; //default local quarks
    else if(m_class == UC_ELEMENT)
      ctype << "<EC>";
    else
      assert(0);

    return ctype.str();
  } //getImmediateStorageTypeAsString

  void UlamTypeClass::genUlamTypeReadDefinitionForC(File * fp)
  {
    if(m_class == UC_QUARK)
      return genUlamTypeQuarkReadDefinitionForC(fp);
    else if(m_class == UC_ELEMENT)
      return genUlamTypeElementReadDefinitionForC(fp);
    else
      assert(0);
  } //genUlamTypeReadDefinitionForC

  void UlamTypeClass::genUlamTypeWriteDefinitionForC(File * fp)
  {
    if(m_class == UC_QUARK)
      return genUlamTypeQuarkWriteDefinitionForC(fp);
    else if(m_class == UC_ELEMENT)
      return genUlamTypeElementWriteDefinitionForC(fp);
    else
      assert(0);
  } //genUlamTypeWriteDefinitionForC

  const std::string UlamTypeClass::castMethodForCodeGen(UTI nodetype)
  {
    std::ostringstream rtnMethod;
    UlamType * nut = m_state.getUlamTypeByIndex(nodetype);
    //base types e.g. Int, Bool, Unary, Foo, Bar..
    u32 sizeByIntBitsToBe = getTotalWordSize();
    u32 sizeByIntBits = nut->getTotalWordSize();

    if(sizeByIntBitsToBe != sizeByIntBits)
      {
	std::ostringstream msg;
	msg << "Casting different word sizes; " << sizeByIntBits;
	msg << ", Value Type and size was: ";
	msg << nut->getUlamTypeName().c_str() << ", to be: ";
	msg << sizeByIntBitsToBe << " for type: ";
	msg << getUlamTypeName().c_str();
	MSG(m_state.getFullLocationAsString(m_state.m_locOfNextLineText).c_str(),msg.str().c_str(), ERR);
      }

    if(m_class != UC_ELEMENT)
      {
	std::ostringstream msg;
	msg << "Quarks only cast 'toInt': value type and size was: ";
	msg << nut->getUlamTypeName().c_str();
	msg << ", to be: " << getUlamTypeName().c_str();
	MSG(m_state.getFullLocationAsString(m_state.m_locOfNextLineText).c_str(),msg.str().c_str(), ERR);
      }

    //e.g. casting an element to an element, redundant and not supported: Element96ToElement96?
    if(nodetype != UAtom)
      {
	std::ostringstream msg;
	msg << "Attempting to illegally cast a non-atom type to an element: ";
	msg << "value type and size was: ";
	msg << nut->getUlamTypeName().c_str() << ", to be: " << getUlamTypeName().c_str();
	MSG(m_state.getFullLocationAsString(m_state.m_locOfNextLineText).c_str(),msg.str().c_str(), ERR);
      }

    rtnMethod << "_" << nut->getUlamTypeNameOnly().c_str() << sizeByIntBits;
    rtnMethod << "ToElement" << sizeByIntBitsToBe;
    return rtnMethod.str();
  } //castMethodForCodeGen

  void UlamTypeClass::genUlamTypeMangledDefinitionForC(File * fp)
  {
    if(m_class == UC_QUARK)
      return genUlamTypeQuarkMangledDefinitionForC(fp);
    else if(m_class == UC_ELEMENT)
      return genUlamTypeElementMangledDefinitionForC(fp);
    else
      assert(0);
  } //genUlamTypeMangledDefinitionForC

  void UlamTypeClass::genUlamTypeQuarkMangledDefinitionForC(File * fp)
  {
    assert(m_class == UC_QUARK);

    //class instance idx is always the scalar uti
    UTI scalaruti =  m_key.getUlamKeyTypeSignatureClassInstanceIdx();
    const std::string scalarmangledName = m_state.getUlamTypeByIndex(scalaruti)->getUlamTypeMangledName();

    m_state.m_currentIndentLevel = 0;
    const std::string mangledName = getUlamTypeImmediateMangledName();
    const std::string automangledName = getUlamTypeImmediateAutoMangledName();
    std::ostringstream  ud;
    ud << "Ud_" << automangledName; //d for define (p used for atomicparametrictype)
    std::string udstr = ud.str();

    m_state.indent(fp);
    fp->write("#ifndef ");
    fp->write(udstr.c_str());
    fp->write("\n");

    m_state.indent(fp);
    fp->write("#define ");
    fp->write(udstr.c_str());
    fp->write("\n");

    m_state.indent(fp);
    fp->write("namespace MFM{\n");
    fp->write("\n");

    m_state.m_currentIndentLevel++;

    //forward declaration of quark (before struct!)
    m_state.indent(fp);
    fp->write("template<class EC, u32 POS> class ");
    fp->write(scalarmangledName.c_str());
    fp->write(";  //forward\n\n");

    m_state.indent(fp);
    fp->write("template<class EC, u32 POS>\n"); //quark auto per POS

    m_state.indent(fp);
    fp->write("struct ");
    fp->write(automangledName.c_str());
    fp->write(" : public AutoRefBase<EC>");
    fp->write("\n");
    m_state.indent(fp);
    fp->write("{\n");

    m_state.m_currentIndentLevel++;

    //typedef atomic parameter type inside struct
    m_state.indent(fp);
    fp->write("typedef typename EC::ATOM_CONFIG AC;\n");
    m_state.indent(fp);
    fp->write("typedef typename AC::ATOM_TYPE T;\n");
    m_state.indent(fp);
    fp->write("enum { BPA = AC::BITS_PER_ATOM };\n");
    fp->write("\n");

    //quark typedef
    m_state.indent(fp);
    fp->write("typedef ");
    fp->write(scalarmangledName.c_str());
    fp->write("<EC, ");
    fp->write("T::ATOM_FIRST_STATE_BIT");
    fp->write("> Us;\n");

    s32 len = getTotalBitSize();

    m_state.indent(fp);
    fp->write("typedef AtomicParameterType");
    fp->write("<EC"); //BITSPERATOM
    fp->write(", ");
    fp->write(getUlamTypeVDAsStringForC().c_str());
    fp->write(", ");
    fp->write_decimal_unsigned(len); //includes arraysize
    fp->write("u, ");
    fp->write("T::ATOM_FIRST_STATE_BIT");
    fp->write("> ");
    fp->write(" Up_Us;\n");

    // see UlamClass.h for AutoRefBase
    //constructor for conditional-as (auto)
    m_state.indent(fp);
    fp->write(automangledName.c_str());
    fp->write("(T& targ, u32 idx) : AutoRefBase<EC>(targ, idx) { }\n");

    //constructor for chain of autorefs (e.g. memberselect with array item)
    m_state.indent(fp);
    fp->write(automangledName.c_str());
    fp->write("(AutoRefBase<EC>& arg, u32 idx) : AutoRefBase<EC>(arg, idx) { }\n");

    //read 'entire quark' method
    genUlamTypeQuarkReadDefinitionForC(fp);

    //write 'entire quark' method
    genUlamTypeQuarkWriteDefinitionForC(fp);

    // aref/aset calls generated inline for immediates.
    if(isCustomArray())
      {
	m_state.indent(fp);
	fp->write("/* a custom array, btw ('Us' has aref, aset methods) */\n");
      }

    m_state.m_currentIndentLevel--;
    m_state.indent(fp);
    fp->write("};\n");

    m_state.m_currentIndentLevel--;
    m_state.indent(fp);
    fp->write("} //MFM\n");

    m_state.indent(fp);
    fp->write("#endif /*");
    fp->write(udstr.c_str());
    fp->write(" */\n\n");
  } //genUlamTypeQuarkMangledDefinitionForC

  void UlamTypeClass::genUlamTypeQuarkReadDefinitionForC(File * fp)
  {
    m_state.indent(fp);
    fp->write("const ");
    fp->write(getTmpStorageTypeAsString().c_str()); //s32 or u32
    fp->write(" read() const { ");
    //fp->write("assert(Us::THE_INSTANCE.GetPos() == (AutoRefBase<EC>::getPosOffset() + T::ATOM_FIRST_STATE_BIT)); "); //assert temporary XXX
    fp->write("return Up_Us::");
    fp->write(readMethodForCodeGen().c_str());
    fp->write("(AutoRefBase<EC>::getBits()); ");
    fp->write("}\n");

    if(!isScalar())
      {
	//reads an element of array
	//2nd argument generated for compatibility with underlying method
	m_state.indent(fp);
	fp->write("const ");
	fp->write(getArrayItemTmpStorageTypeAsString().c_str()); //s32 or u32
	fp->write(" readArrayItem(");
	fp->write("const u32 index, const u32 itemlen) const { return ");
	fp->write("AutoRefBase<EC>::readArrayItem");
	fp->write("(index, itemlen); }\n");
      }
  } //genUlamTypeQuarkReadDefinitionForC

  void UlamTypeClass::genUlamTypeQuarkWriteDefinitionForC(File * fp)
  {
    m_state.indent(fp);
    fp->write("void write(const ");
    fp->write(getTmpStorageTypeAsString().c_str()); //s32 or u32
    fp->write(" v) { ");
    //fp->write("assert(Us::THE_INSTANCE.GetPos() == ( AutoRefBase<EC>::getPosOffset() + T::ATOM_FIRST_STATE_BIT)); "); //assert temporary XXX
    fp->write("Up_Us::");
    fp->write(writeMethodForCodeGen().c_str());
    fp->write("(AutoRefBase<EC>::getBits(), v); ");
    fp->write("}\n");

    if(!isScalar())
      {
	// writes an element of array
	//3rd argument generated for compatibility with underlying method
	m_state.indent(fp);
	fp->write("void writeArrayItem(const ");
	fp->write(getArrayItemTmpStorageTypeAsString().c_str()); //s32 or u32
	fp->write(" v, const u32 index, const u32 itemlen) { ");
	fp->write("AutoRefBase<EC>::writeArrayItem");
	fp->write("(v, index, itemlen); }\n");
      }
  } //genUlamTypeQuarkWriteDefinitionForC

void UlamTypeClass::genUlamTypeElementMangledDefinitionForC(File * fp)
  {
    assert(isScalar());

    m_state.m_currentIndentLevel = 0;
    const std::string mangledName = getUlamTypeImmediateMangledName();
    const std::string automangledName = getUlamTypeImmediateAutoMangledName();
    std::ostringstream  ud;
    ud << "Ud_" << automangledName; //d for define (p used for atomicparametrictype)
    std::string udstr = ud.str();

    m_state.indent(fp);
    fp->write("#ifndef ");
    fp->write(udstr.c_str());
    fp->write("\n");

    m_state.indent(fp);
    fp->write("#define ");
    fp->write(udstr.c_str());
    fp->write("\n");

    m_state.indent(fp);
    fp->write("namespace MFM{\n");
    fp->write("\n");

    m_state.m_currentIndentLevel++;

    //forward declaration of element (before struct!)
    m_state.indent(fp);
    fp->write("template<class EC> class ");
    fp->write(getUlamTypeMangledName().c_str());
    fp->write(";  //forward\n\n");

    m_state.indent(fp);
    fp->write("template<class EC>\n");

    m_state.indent(fp);
    fp->write("struct ");
    fp->write(automangledName.c_str());
    fp->write(" : public AutoRefBase<EC>");
    fp->write("\n");

    m_state.indent(fp);
    fp->write("{\n");

    m_state.m_currentIndentLevel++;

    //typedef atomic parameter type inside struct
    m_state.indent(fp);
    fp->write("typedef typename EC::ATOM_CONFIG AC;\n");
    m_state.indent(fp);
    fp->write("typedef typename AC::ATOM_TYPE T;\n");
    m_state.indent(fp);
    fp->write("enum { BPA = AC::BITS_PER_ATOM };\n");
    m_state.indent(fp);
    fp->write("typedef BitField<BitVector<BPA>, VD::BITS, T::ATOM_FIRST_STATE_BIT, 0> BFTYP;\n");
    fp->write("\n");

    //element typedef
    m_state.indent(fp);
    fp->write("typedef ");
    fp->write(getUlamTypeMangledName().c_str());
    fp->write("<EC> Us;\n");
    fp->write("\n");

    //constructor for conditional-as (auto)
    m_state.indent(fp);
    fp->write(automangledName.c_str());
    fp->write("(T& targ) : AutoRefBase<EC>(targ, 0u) { }\n");

    //constructor for chain of autorefs (e.g. memberselect with array item)
    m_state.indent(fp);
    fp->write(automangledName.c_str());
    fp->write("(AutoRefBase<EC>& arg) : AutoRefBase<EC>(arg, 0) { }\n");

    //default destructor (for completeness)
    m_state.indent(fp);
    fp->write("~");
    fp->write(automangledName.c_str());
    fp->write("() {}\n");

    //read 'entire atom' method
    genUlamTypeElementReadDefinitionForC(fp);

    //write 'entire atom' method
    genUlamTypeElementWriteDefinitionForC(fp);

    m_state.m_currentIndentLevel--;
    m_state.indent(fp);
    fp->write("};\n");

    m_state.m_currentIndentLevel--;
    m_state.indent(fp);
    fp->write("} //MFM\n");

    m_state.indent(fp);
    fp->write("#endif /*");
    fp->write(udstr.c_str());
    fp->write(" */\n\n");
  } //genUlamTypeElementMangledDefinitionForC

  void UlamTypeClass::genUlamTypeElementReadDefinitionForC(File * fp)
  {
    // arrays are handled separately
    assert(isScalar());

    //not an array
    m_state.indent(fp);
    fp->write("const ");
    fp->write(getTmpStorageTypeAsString().c_str()); //T
    fp->write(" read() const { return AutoRefBase<EC>::read(); /* entire element */ }\n");

    m_state.indent(fp);
    fp->write("const u32 readTypeField()");
    fp->write("{ return BFTYP::Read(AutoRefBase<EC>::getRef()); }\n");
  } //genUlamTypeElementReadDefinitionForC

  void UlamTypeClass::genUlamTypeElementWriteDefinitionForC(File * fp)
  {
    // arrays are handled separately
    assert(isScalar());

    // write must be scalar; ref param to avoid excessive copying

    m_state.indent(fp);
    fp->write("void writeTypeField(const u32 v)");
    fp->write("{ BFTYP::Write(AutoRefBase<EC>::getRef(), v); }\n");
  } //genUlamTypeElementWriteDefinitionForC

  const std::string UlamTypeClass::readArrayItemMethodForCodeGen()
  {
    if(isCustomArray())
      return m_state.getCustomArrayGetMangledFunctionName();
    return UlamType::readArrayItemMethodForCodeGen();
  }

  const std::string UlamTypeClass::writeArrayItemMethodForCodeGen()
  {
    if(isCustomArray())
      return m_state.getCustomArraySetMangledFunctionName();
    return UlamType::writeArrayItemMethodForCodeGen();
  }

  //actually builds the immediate definition, inheriting from auto
  void UlamTypeClass::genUlamTypeMangledAutoDefinitionForC(File * fp)
  {
    //class instance idx is always the scalar uti
    UTI scalaruti =  m_key.getUlamKeyTypeSignatureClassInstanceIdx();
    const std::string scalarmangledName = m_state.getUlamTypeByIndex(scalaruti)->getUlamTypeMangledName();

    const std::string mangledName = getUlamTypeImmediateMangledName();
    const std::string automangledName = getUlamTypeImmediateAutoMangledName();

    std::ostringstream  ud;
    ud << "Ud_" << mangledName; //d for define (p used for atomicparametrictype)
    std::string udstr = ud.str();

    m_state.m_currentIndentLevel = 0;

    m_state.indent(fp);
    fp->write("#ifndef ");
    fp->write(udstr.c_str());
    fp->write("\n");

    m_state.indent(fp);
    fp->write("#define ");
    fp->write(udstr.c_str());
    fp->write("\n");

    m_state.indent(fp);
    fp->write("namespace MFM{\n");
    fp->write("\n");

    m_state.m_currentIndentLevel++;

    m_state.indent(fp);
    fp->write("template<class EC>\n");
    m_state.indent(fp);
    fp->write("struct ");
    fp->write(mangledName.c_str());
    fp->write(" : public ");
    fp->write(automangledName.c_str());
    if(m_class == UC_QUARK)
      fp->write("<EC, EC::ATOM_CONFIG::ATOM_TYPE::ATOM_FIRST_STATE_BIT>\n");
    else
      fp->write("<EC>\n");

    m_state.indent(fp);
    fp->write("{\n");

    m_state.m_currentIndentLevel++;

    //typedef atomic parameter type inside struct
    m_state.indent(fp);
    fp->write("typedef typename EC::ATOM_CONFIG AC;\n");
    m_state.indent(fp);
    fp->write("typedef typename AC::ATOM_TYPE T;\n");
    m_state.indent(fp);
    fp->write("enum { BPA = AC::BITS_PER_ATOM };\n");
    fp->write("\n");

    //storage here in atom
    m_state.indent(fp);
    fp->write("T m_stg;  //storage here!\n");

    if(m_class == UC_QUARK)
      {
	s32 len = getTotalBitSize();

	//quark typedef
	m_state.indent(fp);
	fp->write("typedef ");
	fp->write(scalarmangledName.c_str());
	fp->write("<EC, ");
	fp->write("T::ATOM_FIRST_STATE_BIT");
	fp->write("> Us;\n");

	m_state.indent(fp);
	fp->write("typedef AtomicParameterType");
	fp->write("<EC"); //BITSPERATOM
	fp->write(", ");
	fp->write(getUlamTypeVDAsStringForC().c_str());
	fp->write(", ");
	fp->write_decimal_unsigned(len); //includes arraysize
	fp->write("u, ");
	fp->write("T::ATOM_FIRST_STATE_BIT");
	fp->write("> ");
	fp->write(" Up_Us;\n");

	//default constructor (used by local vars)
	//(unlike element) call build default in case of initialized data members
	u32 dqval = 0;
	bool hasDQ = genUlamTypeDefaultQuarkConstant(fp, dqval);

	m_state.indent(fp);
	fp->write(mangledName.c_str());
	fp->write("() : ");
	fp->write(automangledName.c_str());
	fp->write("<EC, T::ATOM_FIRST_STATE_BIT>(m_stg, 0), ");
	fp->write("m_stg(T::ATOM_UNDEFINED_TYPE) { "); //for immediate quarks
	if(hasDQ)
	  {
	    if(isScalar())
	      {
		fp->write(automangledName.c_str());
		fp->write("<EC, T::ATOM_FIRST_STATE_BIT>::");
		fp->write("write(DEFAULT_QUARK);");
	      }
	    else
	      {
		//very packed array
		if(len <= MAXBITSPERINT)
		  {
		    u32 bitsize = getBitSize();
		    u32 dqarrval = 0;
		    u32 pos = 0;
		    u32 mask = _GetNOnes32((u32) bitsize);
		    u32 arraysize = getArraySize();
		    dqval &= mask;
		    //similar to build default quark value in NodeVarDeclDM
		    for(u32 j = 1; j <= arraysize; j++)
		      {
			dqarrval |= (dqval << (len - (pos + (j * bitsize))));
		      }

		    std::ostringstream qdhex;
		    qdhex << "0x" << std::hex << dqarrval;

		    fp->write(automangledName.c_str());
		    fp->write("<EC, T::ATOM_FIRST_STATE_BIT>::");
		    fp->write("write( ");
		    fp->write(qdhex.str().c_str());
		    fp->write(");");
		  }
		else
		  {
		    fp->write("u32 n = ");
		    fp->write_decimal(getArraySize());
		    fp->write("u; while(n--) { ");
		    fp->write(automangledName.c_str());
		    fp->write("<EC, T::ATOM_FIRST_STATE_BIT>::");
		    fp->write("AutoRefBase<EC>::writeArrayItem(DEFAULT_QUARK, n, ");
		    fp->write_decimal_unsigned(getBitSize());
		    fp->write("u); }");
		  }
	      }
	  }
	fp->write(" }\n");

	//constructor here (used by const tmpVars)
	m_state.indent(fp);
	fp->write(mangledName.c_str());
	fp->write("(const ");
	fp->write(getTmpStorageTypeAsString().c_str()); //s32 or u32
	fp->write(" d) : ");
	fp->write(automangledName.c_str());
	fp->write("<EC, T::ATOM_FIRST_STATE_BIT>(m_stg, 0), ");
	fp->write("m_stg(T::ATOM_UNDEFINED_TYPE) { "); //for immediate quarks
	fp->write(automangledName.c_str());
	fp->write("<EC, T::ATOM_FIRST_STATE_BIT>::");
	fp->write("write(d); }\n");

	// assignment constructor
	m_state.indent(fp);
	fp->write(mangledName.c_str());
	fp->write("(const ");
	fp->write(mangledName.c_str());
	fp->write("<EC> & arg) : ");
	fp->write(automangledName.c_str());
	fp->write("<EC, T::ATOM_FIRST_STATE_BIT>(m_stg, 0), ");
	fp->write("m_stg(T::ATOM_UNDEFINED_TYPE) { "); //for immediate quarks
	fp->write(automangledName.c_str());
	fp->write("<EC, T::ATOM_FIRST_STATE_BIT>::");
	fp->write("write(arg.read()); }\n");
      }
    else if(m_class == UC_ELEMENT)
      {
	//default constructor (used by local vars)
	m_state.indent(fp);
	fp->write(mangledName.c_str());
	fp->write("() : ");
	fp->write(automangledName.c_str());
	fp->write("<EC>(m_stg), m_stg() {");
	fp->write(automangledName.c_str());
	fp->write("<EC>::write(");
	fp->write(getUlamTypeMangledName().c_str());
	fp->write("<EC>");
	fp->write("::THE_INSTANCE");
	fp->write(".GetDefaultAtom()"); //returns object of type T
	fp->write("); }\n");

	//constructor here (used by const tmpVars); ref param to avoid excessive copying
	m_state.indent(fp);
	fp->write(mangledName.c_str());
	fp->write("(const ");
	fp->write(getTmpStorageTypeAsString().c_str()); //T
	fp->write("& d) : ");
	fp->write(automangledName.c_str());
	fp->write("<EC>(m_stg), m_stg(d) ");
	fp->write("{ }\n");

	//assignment constructor
	m_state.indent(fp);
	fp->write(mangledName.c_str());
	fp->write("(const ");
	fp->write(mangledName.c_str());
	fp->write("<EC> & arg) : ");
	fp->write(automangledName.c_str());
	fp->write("<EC>(m_stg), m_stg() { ");
	fp->write(automangledName.c_str());
	fp->write("<EC>::write(arg.read()); }\n");

	//default destructor (for completeness)
	m_state.indent(fp);
	fp->write("~");
	fp->write(mangledName.c_str());
	fp->write("() {}\n");
      }
    else
      assert(0);

    m_state.m_currentIndentLevel--;
    m_state.indent(fp);
    fp->write("};\n");

    m_state.m_currentIndentLevel--;
    m_state.indent(fp);
    fp->write("} //MFM\n");

    m_state.indent(fp);
    fp->write("#endif /*");
    fp->write(udstr.c_str());
    fp->write(" */\n\n");
  } //genUlamTypeMangledAutoDefinitionForC

  void UlamTypeClass::genUlamTypeMangledImmediateModelParameterDefinitionForC(File * fp)
  {
    assert(0); //only primitive types
  }

  bool UlamTypeClass::genUlamTypeDefaultQuarkConstant(File * fp, u32& dqref)
  {
    bool rtnb = false;
    if(m_class == UC_QUARK)
      {
	//always the scalar.
	if(m_state.getDefaultQuark(m_key.getUlamKeyTypeSignatureClassInstanceIdx(), dqref))
	  {
	    std::ostringstream qdhex;
	    qdhex << "0x" << std::hex << dqref;

	    m_state.indent(fp);
	    fp->write("static const u32 DEFAULT_QUARK = ");
	    fp->write(qdhex.str().c_str());
	    fp->write("; //=");
	    fp->write_decimal_unsigned(dqref);
	    fp->write("u\n\n");
	    rtnb = true;
	  }
      }
    return rtnb;
  } //genUlamTypeDefaultQuarkConstant

} //end MFM
