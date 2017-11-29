
// See sbmlreader.h for a description of the contents of this file.

#include "stdafx.h"

#ifdef ENABLE_SBML

#include <set>
#include <sstream>
#include <sbml/SBMLTypes.h>

#include "sbmlreader.h"
#include "platform.h"
#include "reaction.h"

namespace sgns2 {

namespace ReactionConstantProps {
	enum Type {
		NORMAL,
		CONSTANT,
		COMPLEX
	};
};
	
ReactionConstantProps::Type extractReactionConstant( const ASTNode *factor, std::string &rateLaw, const ::Reaction *rxn ) {
	switch( factor->getType() ) {
	case AST_TIMES: {
		bool isConst = true;
		for( unsigned j = 0; j < factor->getNumChildren(); j++ ) {
			ASTNode *node = factor->getChild( j );
			ReactionConstantProps::Type subNodeProps = extractReactionConstant( node, rateLaw, rxn );
			if( subNodeProps == ReactionConstantProps::COMPLEX )
				return ReactionConstantProps::COMPLEX;
			isConst &= subNodeProps == ReactionConstantProps::CONSTANT;
		}
		return isConst ? ReactionConstantProps::CONSTANT : ReactionConstantProps::NORMAL; }
	case AST_REAL:
	case AST_INTEGER: {
		char val[64];
		rateLaw +=  " * ";
		if( factor->isInteger() )
			snprintf( val, sizeof(val), "%d", factor->getInteger() );
		if( factor->isReal() )
			snprintf( val, sizeof(val), "%f", factor->getReal() );
		rateLaw += val;
		return ReactionConstantProps::CONSTANT; }
	case AST_NAME: {
		const ListOfSpeciesReferences *reactants = rxn->getListOfReactants();
		for( unsigned k = 0; k < reactants->size(); k++ ) {
			const SpeciesReference *reactant = static_cast<const SpeciesReference*>(reactants->get( k ));
			if( reactant->getSpecies() == factor->getName() ) {
				return ReactionConstantProps::NORMAL;
			}
		}
		const ListOfLocalParameters *localParamList = rxn->getKineticLaw()->getListOfLocalParameters();
		for( unsigned k = 0; k < localParamList->size(); k++ ) {
			const LocalParameter *localParam = static_cast<const LocalParameter*>(localParamList->get( k ));
			if( localParam->getId() == factor->getName() ) {
				rateLaw +=  " * ";
				char val[64];
				snprintf( val, sizeof(val), "%f", localParam->getValue() );
				rateLaw += val;
				return ReactionConstantProps::CONSTANT;
			}
		}
		rateLaw +=  " * ";
		rateLaw += factor->getName();
		return ReactionConstantProps::CONSTANT; }
	default:
		return ReactionConstantProps::COMPLEX;
	}
}

void addExtraSpeciesInLaw( const ASTNode *node, parse::ParseListener *target, std::string &rateFunction, std::set<std::string> &dontInclude, const ListOfSpecies *allSpecies ) {
	if( node->getType() == AST_NAME ) {
		if( allSpecies->get( node->getName() ) && dontInclude.find( node->getName() ) == dontInclude.end() ) {
			if( !dontInclude.empty() )
				rateFunction += ",";
			rateFunction += node->getName();
			target->selectChemical( node->getName(), true );
			target->newReactant( 0 );
			dontInclude.insert( node->getName() ); 
		}
	}

	for( unsigned j = 0; j < node->getNumChildren(); j++ ) {
		addExtraSpeciesInLaw( node->getChild( j ), target, rateFunction, dontInclude, allSpecies );
	}
}

void importSBMLDoc( SBMLDocument* document, parse::Parser *parser, parse::ParseListener *target ) {
	unsigned int level   = document->getLevel  ();
	unsigned int version = document->getVersion();
	if( level > 3 || (level == 3 && version > 1) || (level == 2 && version > 4) || (level == 1 && version > 5) )
		parser->warning( "This SBML document is in a newer format than supported by SGNS2. Errors may occur." );

	Model* model = document->getModel();
	target->createCompartmentType( "_C" );
	target->selectCompartmentType( "_C" );

	std::string docLocation = "SBML File";

	const ListOfEvents *eventList = model->getListOfEvents();
	if( eventList->size() > 0 )
		parser->warning( "SGNS2 does not support SBML Events yet. The model may behave incorrectly." );

	const ListOfRules *ruleList = model->getListOfRules();
	if( ruleList->size() > 0 )
		parser->warning( "SGNS2 does not support SBML Rules yet. The model may behave incorrectly." );

	const ListOfInitialAssignments *iaList = model->getListOfInitialAssignments();
	if( iaList->size() > 0 )
		parser->warning( "SGNS2 does not support SBML Initial Assignments yet. The model may behave incorrectly." );

	const ListOfParameters *paramList = model->getListOfParameters();
	for( unsigned i = 0; i < paramList->size(); i++ ) {
		const Parameter *param = paramList->get( i );
		if( !param->getConstant() ) {
			parser->warning( "SGNS2 currently does not support non-constant parameters. The model may behave incorrectly." );
		}
		std::string paramSet( param->getId() );
		paramSet += " = ";
		char val[64];
		snprintf( val, sizeof(val), "%f", param->getValue() );
		paramSet += val;
		parser->parse( docLocation.c_str(), "parameter", paramSet.c_str() );
	}

	const ListOfCompartments *compList = model->getListOfCompartments();
	for( unsigned i = 0; i < compList->size(); i++ ) {
		const ::Compartment *comp = compList->get( i );
		target->selectCompartment( NULL );
		target->instantiateCompartment( comp->getId().c_str() );

		std::string paramSet = comp->getId();
		paramSet += " = ";
		double sz = comp->getSize();
		if( sz == sz ) {
			char val[64];
			snprintf( val, sizeof(val), "%f", comp->getSize() );
			paramSet += val;
			parser->parse( docLocation.c_str(), "parameter", paramSet.c_str() );
		}
	}

	const ListOfSpecies *speciesList = model->getListOfSpecies();
	for( unsigned i = 0; i < speciesList->size(); i++ ) {
		const Species *s = speciesList->get( i );
		target->selectCompartment( NULL );
		if( s->getCompartment().empty() ) {
			target->selectCompartment( s->getCompartment().c_str() );
		} else {
			target->selectCompartment( "_C" );
		}
		if( !s->isSetHasOnlySubstanceUnits() )
			parser->warning( "SGNS2 does not support non-'Substance Units Only' units. The model may behave incorrectly." );
		target->selectChemical( s->getId().c_str(), true );
		if( s->isSetInitialAmount() ) {
			RuntimeDistribution n = RuntimeDistribution::DeltaDistribution( s->getInitialAmount() );
			target->setPopulation( &n, false );
		}
	}

	/*
	const ListOfInitialAssignments *initialAssignments = model->getListOfInitialAssignments();
	for( unsigned i = 0; i < initialAssignments->size(); i++ ) {
		const InitialAssignment *assign = initialAssignments->get( i );
		assign->
	}
	*/

	const ListOfReactions *rxnList = model->getListOfReactions();
	for( unsigned i = 0; i < rxnList->size(); i++ ) {
		const ::Reaction *rxn = rxnList->get( i );
		target->newReaction( rxn->getName().c_str() );

		if( rxn->isSetCompartment() ) {
			target->selectCompartment( NULL );
			std::string inCompChemical = "_at_";
			inCompChemical += rxn->getCompartment();
			target->selectCompartment( rxn->getCompartment().c_str() );
			target->selectChemical( inCompChemical.c_str(), false );
			RuntimeDistribution one = RuntimeDistribution::DeltaDistribution( 1.0 );
			target->setPopulation( &one, false );
			target->newReactant( 0 );
			RateFunction linear = RateFunction::Linear();
			target->setRate( &linear );
		}

		target->selectCompartment( NULL );
		target->selectCompartment( "_C" );

		const ListOfSpeciesReferences *reactants = rxn->getListOfReactants();
		const ListOfSpeciesReferences *modifiers = rxn->getListOfModifiers();
		const ListOfSpeciesReferences *products = rxn->getListOfProducts();

		double c = 1.0;
		const KineticLaw *law = rxn->getKineticLaw();
		std::string rateLaw = "1";
		ReactionConstantProps::Type cType = extractReactionConstant( law->getMath(), rateLaw, rxn );

		for( unsigned j = 0; j < reactants->size(); j++ ) {
			const SpeciesReference *reactant = static_cast<const SpeciesReference*>(reactants->get( j ));
			const Species *s = speciesList->get( reactant->getSpecies() );
			bool isBoundary = s->isSetBoundaryCondition() && s->getBoundaryCondition();

			target->selectChemical( reactant->getSpecies().c_str(), true );
			int stoich = (int)reactant->getStoichiometry();
			target->newReactant( isBoundary ? 0 : stoich );
			RateFunction rf;
			if( cType == ReactionConstantProps::CONSTANT ) {
				rf = RateFunction::Unit();
			} else if( stoich < 1 ) {
				rf = RateFunction::Linear();
			} else {
				rf = BasicRateFunction::GilH( stoich );
			}
			target->setRate( &rf );
		}

		for( unsigned j = 0; j < modifiers->size(); j++ ) {
			const SpeciesReference *modifier = static_cast<const SpeciesReference*>(modifiers->get( j ));
			target->selectChemical( modifier->getSpecies().c_str(), true );
			int stoich = (int)modifier->getStoichiometry();
			target->newReactant( 0 );
			RateFunction rf;
			if( cType == ReactionConstantProps::CONSTANT ) {
				rf = RateFunction::Unit();
			} else if( stoich < 1 ) {
				rf = RateFunction::Linear();
			} else {
				BasicRateFunction::GilH( stoich );
			}
			target->setRate( &rf );
		}

		for( unsigned j = 0; j < products->size(); j++ ) {
			const SpeciesReference *product = static_cast<const SpeciesReference*>(products->get( j ));
			const Species *s = speciesList->get( product->getSpecies() );
			bool isBoundary = s->isSetBoundaryCondition() && s->getBoundaryCondition();
			if (! isBoundary ) {
				target->selectChemical( product->getSpecies().c_str(), true );
				target->newProduct( (int)product->getStoichiometry()  );
			}
		}

		if( cType != ReactionConstantProps::COMPLEX ) {
			parser->getLuaReals( rxn->getName().c_str(), rateLaw.c_str(), &c, 1 );
		} else {
			std::set<std::string> dontInclude;
			rateLaw = "return function(";
			for( unsigned j = 0; j < reactants->size(); j++ ) {
				const SpeciesReference *reactant = static_cast<const SpeciesReference*>(reactants->get( j ));
				if( j > 0 )
					rateLaw += ",";
				rateLaw += reactant->getSpecies();
				dontInclude.insert( reactant->getSpecies() );
			}
			for( unsigned j = 0; j < modifiers->size(); j++ ) {
				const SpeciesReference *modifier = static_cast<const SpeciesReference*>(reactants->get( j ));
				if( j > 0 )
					rateLaw += ",";
				rateLaw += modifier->getSpecies();
				dontInclude.insert( modifier->getSpecies() );
			}
			addExtraSpeciesInLaw( law->getMath(), target, rateLaw, dontInclude, speciesList );
			rateLaw += ")return ";
			rateLaw += law->getFormula();
			rateLaw += ";end";
			double ref = (double)parser->getLuaFunction( docLocation.c_str(), rateLaw.c_str() );
			parser->setHasRuntimeLua();
			target->overrideH( "lua", &ref, 1 );
		}

		target->finishReaction( c );
	}
}

void verifySBMLAvailable( parse::Parser *parser ) {
	HMODULE hm = LoadLibrary( "libsbml.dll" );
	if( !hm ) {
		parser->raiseError( "SBML is not available." );
	}
	FreeLibrary( hm );
}

void importSBMLFromFile( const char *filename, parse::Parser *parser, parse::ParseListener *target ) {
	verifySBMLAvailable( parser );

	SBMLDocument* document = readSBMLFromFile( filename );

	if( document->getNumErrors() > 0 ) {
		parser->raiseError( "Failed to read the SBML doc" );
		delete document;
		return;
	}

	importSBMLDoc( document, parser, target );
	delete document;
}

void importSBMLFromStream( std::istream &in, parse::Parser *parser, parse::ParseListener *target ) {
	verifySBMLAvailable( parser );

	std::stringstream ss;
	while( in ) // TODO: Optimize
		ss << (char)in.get();

	SBMLDocument* document = readSBMLFromString( ss.str().c_str() );

	if( document->getNumErrors() > 0 ) {
		parser->raiseError( "Failed to read the SBML doc" );
		delete document;
		return;
	}

	importSBMLDoc( document, parser, target );
	delete document;
}

} // namespace

#endif //ENABLE_SBML
