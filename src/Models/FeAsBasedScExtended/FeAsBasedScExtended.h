/*
Copyright (c) 2009, UT-Battelle, LLC
All rights reserved

[DMRG++, Version 2.0.0]
[by G.A., Oak Ridge National Laboratory]

UT Battelle Open Source Software License 11242008

OPEN SOURCE LICENSE

Subject to the conditions of this License, each
contributor to this software hereby grants, free of
charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), a
perpetual, worldwide, non-exclusive, no-charge,
royalty-free, irrevocable copyright license to use, copy,
modify, merge, publish, distribute, and/or sublicense
copies of the Software.

1. Redistributions of Software must retain the above
copyright and license notices, this list of conditions,
and the following disclaimer.  Changes or modifications
to, or derivative works of, the Software should be noted
with comments and the contributor and organization's
name.

2. Neither the names of UT-Battelle, LLC or the
Department of Energy nor the names of the Software
contributors may be used to endorse or promote products
derived from this software without specific prior written
permission of UT-Battelle.

3. The software and the end-user documentation included
with the redistribution, with or without modification,
must include the following acknowledgment:

"This product includes software produced by UT-Battelle,
LLC under Contract No. DE-AC05-00OR22725  with the
Department of Energy."

*********************************************************
DISCLAIMER

THE SOFTWARE IS SUPPLIED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER, CONTRIBUTORS, UNITED STATES GOVERNMENT,
OR THE UNITED STATES DEPARTMENT OF ENERGY BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

NEITHER THE UNITED STATES GOVERNMENT, NOR THE UNITED
STATES DEPARTMENT OF ENERGY, NOR THE COPYRIGHT OWNER, NOR
ANY OF THEIR EMPLOYEES, REPRESENTS THAT THE USE OF ANY
INFORMATION, DATA, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED WOULD NOT INFRINGE PRIVATELY OWNED RIGHTS.

*********************************************************


*/
/** \ingroup DMRG */
/*@{*/

/*! \file FeBasedScExtedned.h
 *
 *  An implementation of a Hubbard model for Fe-based superconductors
 *  to use with the DmrgSolver
 *  This extends the FeAsBasedSc model to include JNN and JNNN couplings
 *
 */
#ifndef FEAS_BASED_SC_EX
#define FEAS_BASED_SC_EX
#include "../Models/FeAsModel/ModelFeBasedSc.h"
#include "LinkProductFeAsExtended.h"
#include "ModelCommon.h"

namespace Dmrg {

template<typename ModelBaseType>
class FeAsBasedScExtended : public ModelBaseType {

public:

	typedef ModelFeBasedSc<ModelBaseType> ModelFeAsType;
	typedef typename ModelFeAsType::HilbertState HilbertState;
	typedef typename ModelFeAsType::HilbertBasisType HilbertBasisType;
	typedef typename ModelBaseType::ModelHelperType ModelHelperType;
	typedef typename ModelBaseType::GeometryType GeometryType;
	typedef typename ModelBaseType::LeftRightSuperType LeftRightSuperType;
	typedef typename ModelBaseType::LinkProductStructType LinkProductStructType;
	typedef typename ModelBaseType::LinkType LinkType;
	typedef typename ModelHelperType::OperatorsType OperatorsType;
	typedef typename OperatorsType::OperatorType OperatorType;
	typedef typename PsimagLite::Vector<OperatorType>::Type VectorOperatorType;
	typedef typename ModelHelperType::RealType RealType;
	typedef typename ModelHelperType::SparseMatrixType SparseMatrixType;
	typedef typename SparseMatrixType::value_type SparseElementType;
	typedef typename OperatorType::Su2RelatedType Su2RelatedType;
	typedef LinkProductFeAsExtended<ModelHelperType> LinkProductType;
	typedef ModelCommon<ModelBaseType,LinkProductType> ModelCommonType;
	typedef	 typename ModelBaseType::MyBasis MyBasis;
	typedef	 typename ModelBaseType::BasisWithOperatorsType MyBasisWithOperators;
	typedef typename MyBasis::BasisDataType BasisDataType;
	typedef typename MyBasis::BlockType BlockType;
	typedef typename ModelBaseType::SolverParamsType SolverParamsType;
	typedef typename ModelBaseType::VectorType VectorType;
	typedef typename ModelBaseType::InputValidatorType InputValidatorType;

	static const SizeType SPIN_UP = ModelFeAsType::SPIN_UP;
	static const SizeType SPIN_DOWN = ModelFeAsType::SPIN_DOWN;

	FeAsBasedScExtended(const SolverParamsType& solverParams,
	                    InputValidatorType& io,
	                    GeometryType const &geometry)
	    : ModelBaseType(io,new ModelCommonType(solverParams,geometry)),
	      modelParameters_(io),
	      geometry_(geometry),
	      modelFeAs_(solverParams,io,geometry),
	      orbitals_(modelParameters_.orbitals)
	{}

	SizeType memResolv(PsimagLite::MemResolv& mres,
	                   SizeType,
	                   PsimagLite::String msg = "") const
	{
		PsimagLite::String str = msg;
		str += "FeAsBasedScExtended";

		const char* start = reinterpret_cast<const char *>(this);
		const char* end = reinterpret_cast<const char *>(&modelParameters_);
		SizeType total = end - start;
		mres.push(PsimagLite::MemResolv::MEMORY_TEXTPTR,
		          total,
		          start,
		          msg + " FeAsBasedScExtended vptr");

		start = end;
		end = start + PsimagLite::MemResolv::SIZEOF_HEAPPTR;
		total += mres.memResolv(&modelParameters_, end-start, str + " modelParameters");

		start = end;
		end = reinterpret_cast<const char *>(&modelFeAs_);
		total += (end - start);
		mres.push(PsimagLite::MemResolv::MEMORY_HEAPPTR,
		          PsimagLite::MemResolv::SIZEOF_HEAPREF,
		          start,
		          str + " ref to geometry");

		mres.memResolv(&geometry_, 0, str + " geometry");

		start = end;
		end = reinterpret_cast<const char *>(&orbitals_);
		total += mres.memResolv(&modelFeAs_, end-start, str + " modelFeAs");

		total += mres.memResolv(&orbitals_,
		                        sizeof(*this) - total,
		                        str + " orbitals");

		return total;
	}

	SizeType hilbertSize(SizeType site) const { return modelFeAs_.hilbertSize(site); }

	void print(std::ostream& os) const { modelFeAs_.print(os); }

	//! find creation operator matrices for (i,sigma) in the natural basis,
	//! find quantum numbers and number of electrons
	//! for each state in the basis
	void setNaturalBasis(typename PsimagLite::Vector<OperatorType> ::Type&creationMatrix,
	                     SparseMatrixType &hamiltonian,
	                     BasisDataType &q,
	                     BlockType const &block,
	                     const RealType& time)  const
	{
		blockIsSize1OrThrow(block);

		modelFeAs_.setNaturalBasis(creationMatrix,hamiltonian,q,block,time);

		// add S^+_i to creationMatrix
		setSplus(creationMatrix,block);

		// add S^z_i to creationMatrix
		setSz(creationMatrix,block);

		// add J_{ij} S^+_i S^-_j + S^-_i S^+_j to Hamiltonia
		addSplusSminus(hamiltonian,creationMatrix,block);

		// add J_{ij} S^z_i S^z_j to Hamiltonian
		addSzSz(hamiltonian,creationMatrix,block);

	}

	//! set creation matrices for sites in block
	void setOperatorMatrices(
	        typename PsimagLite::Vector<OperatorType> ::Type&creationMatrix,
	        BlockType const &block) const
	{
		blockIsSize1OrThrow(block);

		modelFeAs_.setOperatorMatrices(creationMatrix,block);

		// add S^+_i to creationMatrix
		setSplus(creationMatrix,block);

		// add S^z_i to creationMatrix
		setSz(creationMatrix,block);
	}

	PsimagLite::Matrix<SparseElementType> naturalOperator(const PsimagLite::String& what,
	                                                      SizeType site,
	                                                      SizeType dof) const
	{
		BlockType block;
		block.resize(1);
		block[0]=site;
		typename PsimagLite::Vector<OperatorType>::Type creationMatrix;
		setOperatorMatrices(creationMatrix,block);

		if (what=="z") {
			PsimagLite::Matrix<SparseElementType> tmp;
			SizeType x = 2*orbitals_+1;
			crsMatrixToFullMatrix(tmp,creationMatrix[x].data);
			return tmp;
		}

		if (what=="+") {
			PsimagLite::Matrix<SparseElementType> tmp;
			SizeType x = 2*orbitals_;
			crsMatrixToFullMatrix(tmp,creationMatrix[x].data);
			return tmp;
		}

		if (what=="-") { // delta = c^\dagger * c^dagger
			PsimagLite::Matrix<SparseElementType> tmp;
			SizeType x = 2*orbitals_;
			SparseMatrixType tmp2;
			transposeConjugate(tmp2,creationMatrix[x].data);
			crsMatrixToFullMatrix(tmp,tmp2);
			return tmp;
		}
		return modelFeAs_.naturalOperator(what,site,dof);
	}

	//! find all states in the natural basis for a block of n sites
	//! N.B.: HAS BEEN CHANGED TO ACCOMODATE FOR MULTIPLE BANDS
	void setNaturalBasis(typename PsimagLite::Vector<HilbertState>  ::Type&basis,
	                     typename PsimagLite::Vector<SizeType>::Type& q,
	                     const typename PsimagLite::Vector<SizeType>::Type& block) const
	{
		modelFeAs_.setNaturalBasis(basis,q,block);
	}

	void findElectrons(typename PsimagLite::Vector<SizeType>::Type& electrons,
	                   const typename PsimagLite::Vector<HilbertState>::Type& basis,
	                   SizeType site) const
	{
		modelFeAs_.findElectrons(electrons,basis,site);
	}

	virtual void addDiagonalsInNaturalBasis(SparseMatrixType &hmatrix,
	                                        const VectorOperatorType& cm,
	                                        const BlockType& block,
	                                        RealType time,
	                                        RealType factorForDiagonals=1.0)  const
	{
		modelFeAs_.addDiagonalsInNaturalBasis(hmatrix,cm,block,time,factorForDiagonals);
	}

	virtual SizeType maxElectronsOneSpin() const
	{
		return 2 * modelParameters_.orbitals * geometry_.numberOfSites() + 1;
	}

private:

	// add S^+_i to creationMatrix
	void setSplus(
	        typename PsimagLite::Vector<OperatorType> ::Type&creationMatrix,
	        const BlockType& block) const
	{
		SparseMatrixType m;
		cDaggerC(m,creationMatrix,block,1.0,SPIN_UP,SPIN_DOWN);
		Su2RelatedType su2related;
		SizeType offset = 2*orbitals_;
		su2related.source.push_back(offset);
		su2related.source.push_back(offset+1);
		su2related.source.push_back(offset);
		su2related.transpose.push_back(-1);
		su2related.transpose.push_back(-1);
		su2related.transpose.push_back(1);
		su2related.offset = 1;

		OperatorType sPlus(m,1,typename OperatorType::PairType(2,2),-1,
		                   su2related);
		creationMatrix.push_back(sPlus);
	}

	// add S^z_i to creationMatrix
	void setSz(
	        typename PsimagLite::Vector<OperatorType> ::Type&creationMatrix,
	        const BlockType& block) const
	{
		SparseMatrixType m1,m2;
		cDaggerC(m1,creationMatrix,block,0.5,SPIN_UP,SPIN_UP);
		cDaggerC(m2,creationMatrix,block,-0.5,SPIN_DOWN,SPIN_DOWN);
		Su2RelatedType su2related2;
		SparseMatrixType m = m1;
		m += m2;
		OperatorType sz(m,1,typename OperatorType::PairType(2,1),
		                1.0/sqrt(2.0),su2related2);
		creationMatrix.push_back(sz);
	}

	// add S^+_i to creationMatrix
	void cDaggerC(
	        SparseMatrixType& sum,
	        const typename PsimagLite::Vector<OperatorType> ::Type&creationMatrix,
	        const BlockType&,
	        RealType value,
	        SizeType spin1,
	        SizeType spin2) const
	{
		SparseMatrixType tmpMatrix,tmpMatrix2;
		for (SizeType orbital=0;orbital<orbitals_;orbital++) {
			transposeConjugate(tmpMatrix2,
			                   creationMatrix[orbital+spin2*orbitals_].data);
			multiply(tmpMatrix,
			         creationMatrix[orbital+spin1*orbitals_].data,
			        tmpMatrix2);
			multiplyScalar(tmpMatrix2,tmpMatrix,value);
			if (orbital == 0) sum = tmpMatrix2;
			else sum += tmpMatrix2;
		}
	}

	// add J_{ij} S^+_i S^-_j + S^-_i S^+_j to Hamiltonia
	void addSplusSminus(
	        SparseMatrixType &,
	        const typename PsimagLite::Vector<OperatorType> ::Type&,
	        const BlockType&) const
	{
		// nothing if block.size == 1
	}

	// add J_{ij} S^z_i S^z_j to Hamiltonian
	void addSzSz(
	        SparseMatrixType&,
	        const typename PsimagLite::Vector<OperatorType> ::Type&,
	        const BlockType&) const
	{
		// nothing if block.size == 1
	}

	void blockIsSize1OrThrow(const BlockType& block) const
	{
		if (block.size()==1) return;
		throw PsimagLite::RuntimeError("FeAsBasedExtended:: blocks must be of size 1\n");
	}

	//serializr start class FeAsBasedScExtended
	//serializr vptr
	//serializr normal modelParameters_
	ParametersModelFeAs<RealType>  modelParameters_;
	//serializr ref geometry_ start
	const GeometryType& geometry_;
	//serializr normal modelFeAs_
	ModelFeAsType modelFeAs_;
	//serializr normal orbitals_
	SizeType orbitals_;
}; //class FeAsBasedScExtended

} // namespace Dmrg
/*@}*/
#endif // FEAS_BASED_SC_EX

