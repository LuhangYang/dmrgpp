/*
Copyright (c) 2009,-2012 UT-Battelle, LLC
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
/** \file HamiltonianConnection.h
*/

#ifndef HAMILTONIAN_CONNECTION_H
#define HAMILTONIAN_CONNECTION_H

#include "CachedHamiltonianLinks.h"
#include "CrsMatrix.h"
#include "Concurrency.h"
#include <cassert>
#include "ProgramGlobals.h"
#include "HamiltonianAbstract.h"
#include "SuperGeometry.h"
#include "Vector.h"
#include "VerySparseMatrix.h"
#include "ProgressIndicator.h"

namespace Dmrg {

// Keep this class independent of x and y in x = H*y
// For things that depend on x and y use ParallelHamiltonianConnection.h
template<typename LinkProductBaseType>
class HamiltonianConnection {

public:

	typedef typename LinkProductBaseType::GeometryType GeometryType;
	typedef SuperGeometry<GeometryType> SuperGeometryType;
	typedef HamiltonianAbstract<SuperGeometryType> HamiltonianAbstractType;
	typedef typename LinkProductBaseType::ModelHelperType ModelHelperType;
	typedef typename ModelHelperType::RealType RealType;
	typedef typename ModelHelperType::SparseMatrixType SparseMatrixType;
	typedef typename SparseMatrixType::value_type ComplexOrRealType;
	typedef VerySparseMatrix<ComplexOrRealType> VerySparseMatrixType;
	typedef CachedHamiltonianLinks<ComplexOrRealType> CachedHamiltonianLinksType;
	typedef typename ModelHelperType::LinkType LinkType;
	typedef std::pair<SizeType,SizeType> PairType;
	typedef typename GeometryType::AdditionalDataType AdditionalDataType;
	typedef typename PsimagLite::Vector<ComplexOrRealType>::Type VectorType;
	typedef typename PsimagLite::Vector<VectorType>::Type VectorVectorType;
	typedef typename PsimagLite::Concurrency ConcurrencyType;
	typedef PsimagLite::Vector<SizeType>::Type VectorSizeType;
	typedef typename ModelHelperType::LeftRightSuperType LeftRightSuperType;
	typedef typename LeftRightSuperType::ParamsForKroneckerDumperType ParamsForKroneckerDumperType;
	typedef typename LeftRightSuperType::KroneckerDumperType KroneckerDumperType;

	HamiltonianConnection(SizeType m,
	                      const LeftRightSuperType& lrs,
	                      const GeometryType& geometry,
	                      const LinkProductBaseType& lpb,
	                      RealType targetTime,
	                      const ParamsForKroneckerDumperType* pKroneckerDumper)
	    : modelHelper_(m, lrs),
	      superGeometry_(geometry),
	      lpb_(lpb),
	      targetTime_(targetTime),
	      kroneckerDumper_(pKroneckerDumper,lrs,m),
	      progress_("HamiltonianConnection"),
	      lps_(ProgramGlobals::MAX_LPS),
	      systemBlock_(modelHelper_.leftRightSuper().left().block()),
	      envBlock_(modelHelper_.leftRightSuper().right().block()),
	      smax_(*std::max_element(systemBlock_.begin(),systemBlock_.end())),
	      emin_(*std::min_element(envBlock_.begin(),envBlock_.end())),
	      total_(0),
	      hamAbstract_(superGeometry_, smax_, emin_, modelHelper_.leftRightSuper().super().block()),
	      totalOnes_(hamAbstract_.items())
	{
		SizeType nitems = totalOnes_.size();
		for (SizeType x = 0; x < nitems; ++x)
			totalOnes_[x] = cacheConnections(lps_, x, total_);

		if (lps_.typesaved.size() < total_)
			err("getLinkProductStruct: InternalError\n");

		SizeType last = lrs.super().block().size();
		assert(last > 0);
		--last;
		SizeType numberOfSites = geometry.numberOfSites();
		assert(numberOfSites > 0);
		bool superIsReallySuper = (lrs.super().block()[0] == 0 &&
		        lrs.super().block()[last] == numberOfSites - 1);

		if (!superIsReallySuper)
			return; // <-- CONDITIONAL EARLY EXIT HERE

		PsimagLite::OstringStream msg;
		msg<<"LinkProductStructSize="<<total_;
		progress_.printline(msg,std::cout);

		PsimagLite::OstringStream msg2;
		// add left and right contributions
		msg2<<"PthreadsTheoreticalLimitForThisPart="<<(total_ + 2);

		// The theoretical maximum number of pthreads that are useful
		// is equal to C + 2, where
		// C = number of connection = 2*G*M
		// where G = geometry factor
		// and   M = model factor
		// G = 1 for a chain
		// G = leg for a ladder with leg legs. (For instance, G=2 for a 2-leg ladder).
		// M = 2 for the Hubbard model
		//
		// In general, M = \sum_{term=0}^{terms} dof(term)
		// where terms and dof(term) is model dependent.
		// To find M for a model, go to the Model directory and see LinkProduct*.h file.
		// the return of function terms() for terms.
		// For dof(term) see function dof(SizeType term,...).
		// For example, for the HubbardModelOneBand, one must look at
		// src/Model/HubbardOneBand/LinkProductHubbardOneBand.h
		// In that file, terms() returns 1, so that terms=1
		// Therefore there is only one term: term = 0.
		// And dof(0,...) = 2, as you can see in  LinkProductHubbardOneBand.h.
		// Then M = 2.
		progress_.printline(msg2,std::cout);
	}

	void matrixBond(VerySparseMatrixType& matrix) const
	{
		SizeType matrixRank = matrix.rows();
		VerySparseMatrixType matrix2(matrixRank, matrixRank);

		SizeType x = 0;
		ProgramGlobals::ConnectionEnum type = ProgramGlobals::SYSTEM_ENVIRON;
		ComplexOrRealType tmp = 0.0;
		SizeType term = 0;
		SizeType dofs = 0;
		AdditionalDataType additionalData;

		SizeType nitems = totalOnes_.size();
		SizeType total = 0;
		for (SizeType xx = 0; xx < nitems; ++xx) {
			SparseMatrixType matrixBlock(matrixRank, matrixRank);
			for (SizeType i = 0; i < totalOnes_[xx]; ++i) {
				SparseMatrixType mBlock;
				prepare(x, type, tmp, term, dofs, additionalData, total++);
				calcBond(mBlock,
				         x,
				         type,
				         tmp,
				         term,
				         dofs,
				         additionalData);
				matrixBlock += mBlock;
			}

			VerySparseMatrixType vsm(matrixBlock);
			matrix2+=vsm;
		}

		matrix += matrix2;
	}

	void prepare(SizeType& x,
	             ProgramGlobals::ConnectionEnum& type,
	             ComplexOrRealType& tmp,
	             SizeType& term,
	             SizeType& dofs,
	             AdditionalDataType& additionalData,
	             SizeType ix) const
	{
		x = lps_.xsaved[ix];
		type = lps_.typesaved[ix];
		term = lps_.termsaved[ix];
		dofs = lps_.dofssaved[ix];
		tmp = lps_.tmpsaved[ix];
		superGeometry_.fillAdditionalData(additionalData,
		                                  term,
		                                  hamAbstract_.item(x));
	}

	LinkType getKron(const SparseMatrixType** A,
	                 const SparseMatrixType** B,
	                 SizeType xx,
	                 ProgramGlobals::ConnectionEnum type,
	                 const ComplexOrRealType& valuec,
	                 SizeType term,
	                 SizeType dofs,
	                 const AdditionalDataType& additionalData) const
	{
		assert(type == ProgramGlobals::SYSTEM_ENVIRON ||
		       type == ProgramGlobals::ENVIRON_SYSTEM);

		const VectorSizeType& hItems = hamAbstract_.item(xx);
		if (hItems.size() != 2)
			err("getKron(): No Chemical H supported for now\n");

		SizeType i = PsimagLite::indexOrMinusOne(modelHelper_.leftRightSuper().super().block(),
		                                         hItems[0]);
		SizeType j = PsimagLite::indexOrMinusOne(modelHelper_.leftRightSuper().super().block(),
		                                         hItems[1]);

		int offset = modelHelper_.leftRightSuper().left().block().size();
		PairType ops;
		std::pair<char,char> mods('N','C');
		ProgramGlobals::FermionOrBosonEnum fermionOrBoson=ProgramGlobals::FERMION;
		SizeType angularMomentum=0;
		SizeType category=0;
		RealType angularFactor=0;
		bool isSu2 = modelHelper_.isSu2();
		ComplexOrRealType value = valuec;
		lpb_.valueModifier(value,term,dofs,isSu2,additionalData);

		lpb_.setLinkData(term,
		                 dofs,
		                 isSu2,
		                 fermionOrBoson,
		                 ops,
		                 mods,
		                 angularMomentum,
		                 angularFactor,
		                 category,
		                 additionalData);
		LinkType link2(i,
		               j,
		               type,
		               value,
		               dofs,
		               fermionOrBoson,
		               ops,
		               mods,
		               angularMomentum,
		               angularFactor,
		               category);
		SizeType sysOrEnv = (link2.type==ProgramGlobals::SYSTEM_ENVIRON) ?
		            ProgramGlobals::SYSTEM : ProgramGlobals::ENVIRON;
		SizeType envOrSys = (link2.type==ProgramGlobals::SYSTEM_ENVIRON) ?
		            ProgramGlobals::ENVIRON : ProgramGlobals::SYSTEM;
		SizeType site1Corrected =(link2.type==ProgramGlobals::SYSTEM_ENVIRON) ?
		            link2.site1 : link2.site1-offset;
		SizeType site2Corrected =(link2.type==ProgramGlobals::SYSTEM_ENVIRON) ?
		            link2.site2-offset : link2.site2;

		*A = &modelHelper_.reducedOperator(link2.mods.first,
		                                   site1Corrected,
		                                   link2.ops.first,
		                                   sysOrEnv);
		*B = &modelHelper_.reducedOperator(link2.mods.second,
		                                   site2Corrected,
		                                   link2.ops.second,
		                                   envOrSys);

		assert(isNonZeroMatrix(**A));
		assert(isNonZeroMatrix(**B));

		(*A)->checkValidity();
		(*B)->checkValidity();

		return link2;
	}

	LinkType getConnection(const SparseMatrixType** A,
	                       const SparseMatrixType** B,
	                       SizeType ix) const
	{
		SizeType xx = 0;
		ProgramGlobals::ConnectionEnum type;
		SizeType term = 0;
		SizeType dofs = 0;
		ComplexOrRealType tmp = 0.0;
		AdditionalDataType additionalData;
		prepare(xx,type,tmp,term,dofs,additionalData,ix);
		LinkType link2 = getKron(A,B,xx,type,tmp,term,dofs,additionalData);
		return link2;
	}

	KroneckerDumperType& kroneckerDumper() const
	{
		return kroneckerDumper_;
	}

	const ModelHelperType& modelHelper() const { return modelHelper_; }

	SizeType tasks() const {return total_; }

private:

	SizeType cacheConnections(CachedHamiltonianLinksType& lps,
	                          SizeType x,
	                          SizeType& total) const
	{
		const VectorSizeType& hItems = hamAbstract_.item(x);
		assert(superGeometry_.connected(smax_, emin_, hItems));

		ProgramGlobals::ConnectionEnum type = superGeometry_.connectionKind(smax_, hItems);

		assert(type != ProgramGlobals::SYSTEM_SYSTEM &&
		        type != ProgramGlobals::ENVIRON_ENVIRON);

		AdditionalDataType additionalData;
		VectorSizeType edofs(lpb_.dofsAllocationSize());

		SizeType totalOne = 0;
		for (SizeType term = 0; term < superGeometry_.geometry().terms(); ++term) {
			superGeometry_.fillAdditionalData(additionalData, term, hItems);
			SizeType dofsTotal = lpb_.dofs(term, additionalData);
			for (SizeType dofs = 0; dofs < dofsTotal; ++dofs) {
				lpb_.connectorDofs(edofs,
				                   term,
				                   dofs,
				                   additionalData);
				ComplexOrRealType tmp = superGeometry_(smax_,
				                                       emin_,
				                                       hItems,
				                                       edofs,
				                                       term);

				if (tmp == static_cast<RealType>(0.0)) continue;

				tmp = superGeometry_.geometry().vModifier(term, tmp, targetTime_);

				++totalOne;
				assert(lps.typesaved.size() > total);
				lps.xsaved[total] = x;
				lps.typesaved[total]=type;
				lps.tmpsaved[total]=tmp;
				lps.termsaved[total]=term;
				lps.dofssaved[total]=dofs;
				++total;
			}
		}

		return totalOne;
	}

	//! Adds a connector between system and environment
	SizeType calcBond(SparseMatrixType &matrixBlock,
	                  SizeType xx,
	                  ProgramGlobals::ConnectionEnum type,
	                  const ComplexOrRealType& valuec,
	                  SizeType term,
	                  SizeType dofs,
	                  const AdditionalDataType& additionalData) const
	{
		SparseMatrixType const* A = 0;
		SparseMatrixType const* B = 0;
		LinkType link2 = getKron(&A,
		                         &B,
		                         xx,
		                         type,
		                         valuec,
		                         term,
		                         dofs,
		                         additionalData);
		modelHelper_.fastOpProdInter(*A, *B, matrixBlock, link2);

		return matrixBlock.nonZeros();
	}

	bool isNonZeroMatrix(const SparseMatrixType& m) const
	{
		if (m.rows() > 0 && m.cols() > 0) return true;
		return false;
	}

	const ModelHelperType modelHelper_;
	SuperGeometryType superGeometry_;
	const LinkProductBaseType& lpb_;
	RealType targetTime_;
	mutable KroneckerDumperType kroneckerDumper_;
	PsimagLite::ProgressIndicator progress_;
	CachedHamiltonianLinksType lps_;
	const VectorSizeType& systemBlock_;
	const VectorSizeType& envBlock_;
	SizeType smax_;
	SizeType emin_;
	SizeType total_;
	HamiltonianAbstractType hamAbstract_;
	VectorSizeType totalOnes_;
}; // class HamiltonianConnection
} // namespace Dmrg

/*@}*/
#endif // HAMILTONIAN_CONNECTION_H

