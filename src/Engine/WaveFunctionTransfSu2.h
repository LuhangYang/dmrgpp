/*
Copyright (c) 2009-2013, UT-Battelle, LLC
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

/*! \file WaveFunctionTransfSu2.h
 *
 *  This class implements the wave function transformation, see PRL 77, 3633 (1996)
 *
 */

#ifndef WFT_SU2_H
#define WFT_SU2_H

#include "PackIndices.h"
#include "ProgressIndicator.h"
#include "VectorWithOffsets.h" // so that std::norm() becomes visible here
#include "VectorWithOffset.h" // so that std::norm() becomes visible here
#include "WaveFunctionTransfBase.h"
#include "Random48.h"
#include "ParallelWftSu2.h"
#include "MatrixOrIdentity.h"

namespace Dmrg {

template<typename DmrgWaveStructType,typename VectorWithOffsetType>
class WaveFunctionTransfSu2  : public
        WaveFunctionTransfBase<DmrgWaveStructType,VectorWithOffsetType> {

	typedef PsimagLite::PackIndices PackIndicesType;
	typedef WaveFunctionTransfBase<DmrgWaveStructType,VectorWithOffsetType> BaseType;
	typedef typename BaseType::VectorSizeType VectorSizeType;

public:

	typedef typename DmrgWaveStructType::BasisWithOperatorsType BasisWithOperatorsType;
	typedef typename BasisWithOperatorsType::SparseMatrixType SparseMatrixType;
	typedef typename BasisWithOperatorsType::BasisType BasisType;
	typedef typename SparseMatrixType::value_type SparseElementType;
	typedef typename PsimagLite::Vector<SparseElementType>::Type VectorType;
	typedef typename BasisWithOperatorsType::RealType RealType;
	typedef typename BasisType::FactorsType FactorsType;
	typedef typename DmrgWaveStructType::LeftRightSuperType LeftRightSuperType;
	typedef ParallelWftSu2<VectorWithOffsetType,
	        DmrgWaveStructType,
	        LeftRightSuperType> ParallelWftType;
	typedef MatrixOrIdentity<SparseMatrixType> MatrixOrIdentityType;

	static const SizeType INFINITE = ProgramGlobals::INFINITE;
	static const SizeType EXPAND_SYSTEM = ProgramGlobals::EXPAND_SYSTEM;
	static const SizeType EXPAND_ENVIRON = ProgramGlobals::EXPAND_ENVIRON;

	WaveFunctionTransfSu2(
	        const SizeType& stage,
	        const bool& firstCall,
	        const SizeType& counter,
	        const DmrgWaveStructType& dmrgWaveStruct,
	        bool twoSiteDmrg)
	    : stage_(stage),
	      firstCall_(firstCall),
	      counter_(counter),
	      dmrgWaveStruct_(dmrgWaveStruct),
	      twoSiteDmrg_(twoSiteDmrg),
	      progress_("WaveFunctionTransfLocal")
	{
		PsimagLite::OstringStream msg;
		msg<<"Constructing SU(2)";
		progress_.printline(msg,std::cout);
	}

	virtual void transformVector(VectorWithOffsetType& psiDest,
	                             const VectorWithOffsetType& psiSrc,
	                             const LeftRightSuperType& lrs,
	                             const VectorSizeType& nk) const

	{
		if (stage_==EXPAND_ENVIRON) {
			if (firstCall_) {
				transformVector1FromInfinite(psiDest,psiSrc,lrs,nk);
			} else if (counter_==0) {
				transformVector1bounce(psiDest,psiSrc,lrs,nk);
			} else {
				transformVector1(psiDest,psiSrc,lrs,nk);
			}
		}

		if (stage_==EXPAND_SYSTEM) {
			if (firstCall_)
				transformVector2FromInfinite(psiDest,psiSrc,lrs,nk);
			else if (counter_==0)
				transformVector2bounce(psiDest,psiSrc,lrs,nk);
			else transformVector2(psiDest,psiSrc,lrs,nk);
		}
	}

private:

	template<typename SomeVectorType>
	void transformVector1(SomeVectorType& psiDest,
	                      const SomeVectorType& psiSrc,
	                      const LeftRightSuperType& lrs,
	                      const VectorSizeType& nk) const
	{
		if (twoSiteDmrg_)
			return transformVector1FromInfinite(psiDest,psiSrc,lrs,nk);

		typename ParallelWftType::DirectionEnum dir1 = ParallelWftType::DIR_1;
		for (SizeType ii=0;ii<psiDest.sectors();ii++) {
			SizeType i0 = psiDest.sector(ii);
			transformVectorParallel(psiDest,psiSrc,lrs,i0,nk,dir1);
		}
	}

	template<typename SomeVectorType>
	void transformVectorParallel(SomeVectorType& psiDest,
	                             const SomeVectorType& psiSrc,
	                             const LeftRightSuperType& lrs,
	                             SizeType i0,
	                             const VectorSizeType& nk,
	                             typename ParallelWftType::DirectionEnum dir) const
	{
		SizeType total = psiDest.effectiveSize(i0);

		typedef PsimagLite::Parallelizer<ParallelWftType> ParallelizerType;
		ParallelizerType threadedWft(PsimagLite::Concurrency::npthreads,
		                             PsimagLite::MPI::COMM_WORLD);

		ParallelWftType helperWft(psiDest,
		                          psiSrc,
		                          lrs,
		                          i0,
		                          nk,
		                          dmrgWaveStruct_,
		                          dir);

		threadedWft.loopCreate(total, helperWft);
	}

	template<typename SomeVectorType>
	void transformVector1FromInfinite(SomeVectorType& psiDest,
	                                  const SomeVectorType& psiSrc,
	                                  const LeftRightSuperType& lrs,
	                                  const VectorSizeType& nk) const
	{
		for (SizeType ii=0;ii<psiDest.sectors();ii++) {
			SizeType i0 = psiDest.sector(ii);
			transformVector1FromInfinite(psiDest,psiSrc,lrs,i0,nk);
		}
	}

	template<typename SomeVectorType>
	void transformVector1FromInfinite(SomeVectorType& psiDest,
	                                  const SomeVectorType& psiSrc,
	                                  const LeftRightSuperType& lrs,
	                                  SizeType i0,
	                                  const VectorSizeType& nk) const
	{
		SizeType volumeOfNk = ParallelWftType::volumeOf(nk);
		SizeType nip = lrs.super().permutationInverse().size()/
		        lrs.right().permutationInverse().size();

		assert(lrs.left().permutationInverse().size()==volumeOfNk ||
		       lrs.left().permutationInverse().size()==dmrgWaveStruct_.ws.row());
		assert(lrs.right().permutationInverse().size()/volumeOfNk==dmrgWaveStruct_.we.col());

		SizeType start = psiDest.offset(i0);
		SizeType total = psiDest.effectiveSize(i0);
		const FactorsType& factorsSE = lrs.super().getFactors();
		const FactorsType& factorsE = lrs.right().getFactors();

		SparseMatrixType factorsInverseSE, factorsInverseE;
		transposeConjugate(factorsInverseSE,factorsSE);
		transposeConjugate(factorsInverseE,factorsE);

		SparseMatrixType ws(dmrgWaveStruct_.ws);
		SparseMatrixType we(dmrgWaveStruct_.we);
		SparseMatrixType weT;
		transposeConjugate(weT,we);

		PackIndicesType pack1(nip);
		PackIndicesType pack2(volumeOfNk);
		for (SizeType x=0;x<total;x++) {
			SizeType ip,beta,kp,jp;
			SizeType xx = x + start;
			for (int kI = factorsInverseSE.getRowPtr(xx);
			     kI < factorsInverseSE.getRowPtr(xx+1);
			     kI++) {
				pack1.unpack(ip,beta,(SizeType)factorsInverseSE.getCol(kI));
				for (int k2I = factorsInverseE.getRowPtr(beta);
				     k2I < factorsInverseE.getRowPtr(beta+1);
				     k2I++) {
					pack2.unpack(kp,jp,(SizeType)factorsInverseE.getCol(k2I));
					psiDest.fastAccess(i0,x)=factorsInverseSE.getValue(kI)*
					        factorsInverseE.getValue(k2I)*
					        createAux1bFromInfinite(psiSrc,ip,kp,jp,ws,weT,nk);
				}
			}
		}
	}

	template<typename SomeVectorType>
	SparseElementType createAux1bFromInfinite(const SomeVectorType& psiSrc,
	                                          SizeType ip,
	                                          SizeType kp,
	                                          SizeType jp,
	                                          const SparseMatrixType& ws,
	                                          const SparseMatrixType& weT,
	                                          const VectorSizeType& nk) const
	{
		SizeType volumeOfNk = volumeOf(nk);
		SizeType ni=dmrgWaveStruct_.ws.col();
		SizeType nip = dmrgWaveStruct_.lrs.left().permutationInverse().size()/volumeOfNk;
		MatrixOrIdentityType wsRef2(twoSiteDmrg_ && nip>volumeOfNk,ws);
		const FactorsType& factorsS = dmrgWaveStruct_.lrs.left().getFactors();
		const FactorsType& factorsSE = dmrgWaveStruct_.lrs.super().getFactors();
		SparseElementType sum=0;

		SizeType ipkp=ip+kp*nip;
		for (int k2I=factorsS.getRowPtr(ipkp);k2I<factorsS.getRowPtr(ipkp+1);k2I++) {
			SizeType alpha = factorsS.getCol(k2I);
			for (int k=wsRef2.getRowPtr(alpha);k<wsRef2.getRowPtr(alpha+1);k++) {
				SizeType i = wsRef2.getCol(k);
				for (int k2=weT.getRowPtr(jp);k2<weT.getRowPtr(jp+1);k2++) {
					SizeType j = weT.getCol(k2);
					SizeType r = i+j*ni;
					for (int kI=factorsSE.getRowPtr(r);kI<factorsSE.getRowPtr(r+1);kI++) {
						SizeType x = factorsSE.getCol(kI);
						sum += wsRef2.getValue(k)*weT.getValue(k2)*psiSrc.slowAccess(x)*
						        factorsSE.getValue(kI)*factorsS.getValue(k2I);
					}
				}
			}
		}

		return sum;
	}

	const SizeType& stage_;
	const bool& firstCall_;
	const SizeType& counter_;
	const DmrgWaveStructType& dmrgWaveStruct_;
	bool twoSiteDmrg_;
	PsimagLite::ProgressIndicator progress_;

}; // class WaveFunctionTransfSu2
} // namespace Dmrg

/*@}*/
#endif

