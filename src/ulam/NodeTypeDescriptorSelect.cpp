#include <stdlib.h>
#include "NodeTypeDescriptorSelect.h"
#include "CompilerState.h"


namespace MFM {

  NodeTypeDescriptorSelect::NodeTypeDescriptorSelect(const Token& tokarg, UTI auti, NodeTypeDescriptor * node, CompilerState & state) : NodeTypeDescriptor(tokarg, auti, state), m_nodeSelect(node)
  {
    if(m_nodeSelect)
      m_nodeSelect->updateLineage(getNodeNo()); //for unknown subtrees
  }

  NodeTypeDescriptorSelect::NodeTypeDescriptorSelect(const NodeTypeDescriptorSelect& ref) : NodeTypeDescriptor(ref), m_nodeSelect(NULL)
  {
    if(ref.m_nodeSelect)
      m_nodeSelect = (NodeTypeDescriptor *) ref.m_nodeSelect->instantiate();
  }

  NodeTypeDescriptorSelect::~NodeTypeDescriptorSelect()
  {
    delete m_nodeSelect;
    m_nodeSelect = NULL;
  } //destructor

  Node * NodeTypeDescriptorSelect::instantiate()
  {
    return new NodeTypeDescriptorSelect(*this);
  }

  void NodeTypeDescriptorSelect::updateLineage(NNO pno)
  {
    setYourParentNo(pno);
    if(m_nodeSelect)
      m_nodeSelect->updateLineage(getNodeNo());
  } //updateLineage

  bool NodeTypeDescriptorSelect::findNodeNo(NNO n, Node *& foundNode)
  {
    if(Node::findNodeNo(n, foundNode))
      return true;
    if(m_nodeSelect && m_nodeSelect->findNodeNo(n, foundNode))
      return true;
    return false;
  } //findNodeNo

  void NodeTypeDescriptorSelect::printPostfix(File * fp)
  {
    fp->write(getName());
  }

  const char * NodeTypeDescriptorSelect::getName()
  {
    return NodeTypeDescriptor::getName();
  } //getName

  u32 NodeTypeDescriptorSelect::getTypeNameId()
  {
    std::ostringstream nstr;
    if(m_nodeSelect)
      {
	nstr << m_state.m_pool.getDataAsString(m_nodeSelect->getTypeNameId()).c_str();
	nstr << ".";
      }
    nstr << m_state.m_pool.getDataAsString(NodeTypeDescriptor::getTypeNameId()).c_str();
    return m_state.m_pool.getIndexForDataString(nstr.str());
  } //getTypeNameId

  const std::string NodeTypeDescriptorSelect::prettyNodeName()
  {
    return nodeName(__PRETTY_FUNCTION__);
  }

  UTI NodeTypeDescriptorSelect::checkAndLabelType()
  {
    UTI it = getNodeType();
    if(isReadyType())
      return it;

    if(resolveType(it))
      {
	m_ready = true; //set here
	m_uti = it; //given reset here
      }
    setNodeType(it);
    if(it == Hzy) m_state.setGoAgain();
    return getNodeType();
  } //checkAndLabelType


  bool NodeTypeDescriptorSelect::resolveType(UTI& rtnuti)
  {
    bool rtnb = false;
    if(isReadyType())
      {
	rtnuti = getNodeType();
	return true;
      }

    // we are in a "chain" of type selects..
    assert(m_nodeSelect);

    UTI seluti = m_nodeSelect->checkAndLabelType();
    if(m_nodeSelect->isReadyType())
      {
	UlamType * selut = m_state.getUlamTypeByIndex(seluti);
	ULAMTYPE seletyp = selut->getUlamTypeEnum();
	if(seletyp == Class)
	  {
	    u32 tokid = m_state.getTokenDataAsStringId(m_typeTok); //t3806,7,8 t41312
	    // find our id in the "selected" class, must be a typedef (t3267)
	    Symbol * asymptr = NULL;
	    bool hazyKin = false;
	    if(m_state.alreadyDefinedSymbolByAClassOrAncestor(seluti, tokid, asymptr, hazyKin) && !hazyKin)
	      {
		if(asymptr->isTypedef())
		  {
		    UTI auti = asymptr->getUlamTypeIdx();
		    if(m_state.isComplete(auti))
		      {
			rtnuti = auti; //should be mapped already, if necessary
			rtnb = true;
		      }
		    else //t3862
		      {
			UTI mappedUTI;
			if(m_state.mappedIncompleteUTI(seluti, auti, mappedUTI))
			  {
			    std::ostringstream msg;
			    msg << "Substituting Mapped UTI" << mappedUTI << ", ";
			    msg << m_state.getUlamTypeNameBriefByIndex(mappedUTI).c_str();
			    msg << " for incomplete descriptor type: '";
			    msg << m_state.getUlamTypeNameByIndex(auti).c_str();
			    msg << "' UTI" << auti << " while labeling class: ";
			    msg << m_state.getUlamTypeNameBriefByIndex(seluti).c_str();
			    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
			    rtnuti = mappedUTI;
			    rtnb = true;
			  }
			else
			  rtnuti = Hzy;
		      }

		    if(rtnb)
		      {
			if(m_state.hasUnknownTypeInThisClassResolver(auti))
			  {
			    m_state.removeKnownTypeTokenFromThisClassResolver(auti);
			    m_state.cleanupExistingHolder(auti, rtnuti);
			  }
			else if(m_state.isHolder(rtnuti))
			  {
			    rtnuti = Hzy; //not so fast!!
			    rtnb = false;
			  }
		      }
		  }
		else
		  {
		    //error id is not a typedef
		    std::ostringstream msg;
		    msg << "Not a typedef '" << m_state.getTokenDataAsString(m_typeTok).c_str();
		    msg << "' in another class, " ;;
		    msg << m_state.getUlamTypeNameBriefByIndex(seluti).c_str();
		    msg <<" while compiling: ";
		    msg << m_state.getUlamTypeNameBriefByIndex(m_state.getCompileThisIdx()).c_str();
		    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), WARN);
		    rtnuti = Nav; //?
		  }
	      }
	    else
	      {
		//error! id not found
		std::ostringstream msg;
		msg << "Undefined Typedef '" << m_state.getTokenDataAsString(m_typeTok).c_str();
		msg << "' in another class, " ;;
		msg << m_state.getUlamTypeNameByIndex(seluti).c_str();
		msg <<" while compiling: ";
		msg << m_state.getUlamTypeNameBriefByIndex(m_state.getCompileThisIdx()).c_str();
		if(!hazyKin)
		  {
		    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR); //new
		    rtnuti = Nav;
		  }
		else
		  {
		    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
		    rtnuti = Hzy;
		  }
	      }
	  }
	else
	  {
	    //error has to be a class
	    std::ostringstream msg;
	    msg << "Type selected by '" << m_state.getTokenDataAsString(m_typeTok).c_str();
	    msg << "' is NOT another class, " ;
	    msg << m_state.getUlamTypeNameBriefByIndex(seluti).c_str();
	    msg << ", rather a " << UlamType::getUlamTypeEnumAsString(seletyp) << " type,";
	    msg <<" while compiling: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(m_state.getCompileThisIdx()).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	    rtnuti = Nav;
	  }
      }
    else
      rtnuti = Hzy; //else select not ready, so neither are we!!
    return rtnb;
  } //resolveType

  void NodeTypeDescriptorSelect::countNavHzyNoutiNodes(u32& ncnt, u32& hcnt, u32& nocnt)
  {
    NodeTypeDescriptor::countNavHzyNoutiNodes(ncnt, hcnt, nocnt);
    if(m_nodeSelect)
      m_nodeSelect->countNavHzyNoutiNodes(ncnt, hcnt, nocnt);
  }

} //end MFM
