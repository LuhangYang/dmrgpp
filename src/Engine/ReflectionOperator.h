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

/*! \file ReflectionOperator
 *
 *  Problems
 *  - Getting transform from stacked transform at wft
 *  - Getting bases for sys and env from stacks at checkpoint
 *  - non-existant permutations
 *
 */
#ifndef REFLECTION_OPERATOR_H
#define REFLECTION_OPERATOR_H

#include "PackIndices.h" // in PsimagLite
#include "Matrix.h"
#include "ProgressIndicator.h"
#include "ReflectionItem.h"

namespace Dmrg {

template<typename LeftRightSuperType>
class ReflectionOperator {

	typedef PsimagLite::PackIndices PackIndicesType;
	typedef typename LeftRightSuperType::SparseMatrixType
			SparseMatrixType;
	typedef typename LeftRightSuperType::RealType RealType;
	typedef typename SparseMatrixType::value_type ComplexOrRealType;
	typedef ReflectionItem<RealType,ComplexOrRealType> ItemType;
	typedef std::vector<ComplexOrRealType> VectorType;

public:

	ReflectionOperator(const LeftRightSuperType& lrs,size_t n0,bool isEnabled,size_t expandSys)
	: lrs_(lrs),
	  n0_(n0),
	  progress_("ReflectionOperator",0),
	  plusSector_(0),
	  isEnabled_(isEnabled),
	  expandSys_(expandSys),
	  reflectedLeft_(n0_,n0_),
	  reflectedRight_(n0_,n0_)
	{
		size_t counter=0;
		for (size_t i=0;i<reflectedLeft_.rank();i++) {
			reflectedLeft_.setRow(i,counter);
			reflectedLeft_.pushCol(i);
			reflectedLeft_.pushValue(1);
			counter++;
		}
		reflectedLeft_.setRow(reflectedLeft_.rank(),counter);
		reflectedRight_=reflectedLeft_;
	}

	void check(const std::vector<size_t>& sectors)
	{
		if (!isEnabled_) return;
		SparseMatrixType sSuper;
		setS(sSuper);
		updateReflected(sSuper);
		SparseMatrixType sSector;
		extractCurrentSector(sSector,sSuper,sectors);
		std::vector<ItemType> items;
		computeItems(items,sSector);
		std::vector<ItemType> items2;
		makeUnique(items2,items);
		std::cout<<"ITEMS2-------------\n";
		std::cout<<items2;
		assert(items2.size()==sSector.rank());
		setTransform(items2);
	}

	void changeBasis(const PsimagLite::Matrix<ComplexOrRealType>& transform,size_t direction)
	{
		if (!isEnabled_) return;
		SparseMatrixType newreflected;
		if (direction==expandSys_) {
			changeBasis(newreflected,reflectedLeft_,transform);
			reflectedLeft_ = newreflected;
		} else {
			changeBasis(newreflected,reflectedRight_,transform);
			reflectedRight_ = newreflected;
		}
	}


	void transform(SparseMatrixType& matrixA,
		       SparseMatrixType& matrixB,
		       const SparseMatrixType& matrix) const
	 {
		assert(isEnabled_);

		SparseMatrixType rT;
		transposeConjugate(rT,transform_);

		SparseMatrixType tmp;
		multiply(tmp,matrix,rT);
		printFullMatrix(rT,"ConjTranform");
		SparseMatrixType matrix2;
		printFullMatrix(matrix,"OriginalHam");
		multiply(matrix2,transform_,tmp);
		printFullMatrix(transform_,"transform");
		printFullMatrix(matrix2,"FinalHam");
		split(matrixA,matrixB,matrix2);
	 }

	template<typename SomeVectorType>
	void setInitState(const SomeVectorType& initVector,
			  SomeVectorType& initVector1,
			  SomeVectorType& initVector2) const
	{
		assert(isEnabled_);
		size_t minusSector = initVector.size()-plusSector_;
		initVector1.resize(plusSector_);
		initVector2.resize(minusSector);
		for (size_t i=0;i<initVector.size();i++) {
			if (i<plusSector_) initVector1[i]=initVector[i];
			else  initVector2[i-plusSector_]=initVector[i];
		}

	}

	RealType setGroundState(VectorType& gs,
				const RealType& gsEnergy1,
				const VectorType& gsVector1,
				const RealType& gsEnergy2,
				const VectorType& gsVector2) const
	{
		assert(isEnabled_);
		size_t rank = gsVector1.size() + gsVector2.size();
		if (gsEnergy1<=gsEnergy2) {
			setGs(gs,gsVector1,rank,0);
			return gsEnergy1;
		}
		setGs(gs,gsVector2,rank,gsVector1.size());
		return gsEnergy2;
	}

	const LeftRightSuperType& leftRightSuper() const { return lrs_; }

	bool isEnabled() const { return isEnabled_; }

private:

	void changeBasis(SparseMatrixType& newreflected,const SparseMatrixType& reflected,const PsimagLite::Matrix<ComplexOrRealType>& transform)
	{
		newreflected.resize(reflected.rank());
		size_t counter = 0;
		assert(reflected.rank()==transform.n_row());
		assert(reflected.rank()==transform.n_row());
		for (size_t x=0;x<reflected.rank();x++) {
			newreflected.setRow(x,counter);

			for (size_t xprime=0;xprime<transform.n_row();xprime++) {
				ComplexOrRealType wl1 = transform(xprime,x);
				for (int k=reflected.getRowPtr(xprime);k<reflected.getRowPtr(xprime+1);k++) {
					size_t xsecond = reflected.getCol(k);
					ComplexOrRealType r = reflected.getValue(k);
					for (size_t xthird=0;xthird<transform.n_col();xthird++) {
						ComplexOrRealType val = wl1 * r * transform(xsecond,xthird);
						if (isAlmostZero(val)) continue;
						newreflected.pushCol(xthird);
						newreflected.pushValue(val);
						counter++;
					}
				}
			}
		}
		newreflected.setRow(reflected.rank(),counter);
	}

	void setS(SparseMatrixType& snew)
	{
		size_t total = lrs_.super().size();
		size_t ns = lrs_.left().size();
		PackIndicesType pack1(ns);
		PackIndicesType pack2(ns/n0_);
		PackIndicesType pack3(n0_);
		size_t counter = 0;
		RealType sign = 1.0;
		snew.resize(total);
		for (size_t i=0;i<total;i++) {
			snew.setRow(i,counter);
			size_t x=0,y=0;
			pack1.unpack(x,y,lrs_.super().permutation(i));

			size_t x0=0,x1=0;
			pack2.unpack(x0,x1,lrs_.left().permutation(x));

			size_t y0=0,y1=0;
			pack3.unpack(y0,y1,lrs_.right().permutation(y));

			for (int k1=reflectedLeft_.getRowPtr(x0);k1<reflectedLeft_.getRowPtr(x0+1);k1++) {
				size_t x0r = reflectedLeft_.getCol(k1);
				ComplexOrRealType val1 = reflectedLeft_.getValue(k1);
				for (int k2=reflectedRight_.getRowPtr(y1);k2<reflectedRight_.getRowPtr(y1+1);k2++) {
					size_t y1r = reflectedRight_.getCol(k2);
					ComplexOrRealType val2 = reflectedRight_.getValue(k2);
					size_t xprime = pack2.pack(y1r,y0,lrs_.left().permutationInverse());
					size_t yprime = pack3.pack(x1,x0r,lrs_.right().permutationInverse());
					size_t iprime = pack1.pack(xprime,yprime,lrs_.super().permutationInverse());
			//			size_t signCounter=0;
			//			if (x0==3) signCounter++;
			//			if (x1==3) signCounter++;
			//			if (y0==3) signCounter++;
			//			if (y1==3) signCounter++;
			//			sign = 1;
			//			if (signCounter&1) sign=-1;

					snew.pushCol(iprime);
					snew.pushValue(sign*val1*val2);
					counter++;
				}
			}
		}
		snew.setRow(total,counter);
	}

	void extractCurrentSector(SparseMatrixType& sSector,
				  const SparseMatrixType& sSuper,
				  const std::vector<size_t>& sectors) const
	{
		assert(sectors.size()==1);
		size_t m = sectors[0];
		size_t offset = lrs_.super().partition(m);
		size_t total = lrs_.super().partition(m+1)-offset;
		sSector.resize(total);
		size_t counter = 0;
		//bool needsPrinting = true;
		for (size_t i=0;i<total;i++) {
			sSector.setRow(i,counter);
			for (int k=sSuper.getRowPtr(i+offset);k<sSuper.getRowPtr(i+offset+1);k++) {
				size_t col = sSuper.getCol(k);
				ComplexOrRealType val = sSuper.getValue(k);
				bool b = (col>=offset && col<total+offset);
				assert(b);
//				if (!b) {
//					if (needsPrinting) {
//						std::cerr<<"WARNING: extractCurrentSector: reflected_ not following symmetries!!\n";
//						needsPrinting = false;
//					}
//					continue;
//				}
				sSector.pushCol(col-offset);
				sSector.pushValue(val);
				counter++;
			}
		}
		sSector.setRow(total,counter);
	}

	void updateReflected(const SparseMatrixType& sSuper)
	{
		// for each x0 find the reflected of
		// x0 0 0 x0
		// decompose into
		// x0' 0 0 x0'
		// then x0'=reflected(x0)
		size_t ns = lrs_.left().size();
		PackIndicesType pack1(ns);
		size_t ne = lrs_.super().size()/ns;
		assert(ns==ne);
		reflectedLeft_.resize(ns);
		reflectedRight_.resize(ne);
		size_t counter = 0;
		for (size_t x0=0;x0<ns;x0++) {
			reflectedLeft_.setRow(x0,counter);
			reflectedRight_.setRow(x0,counter);
			size_t i = pack1.pack(x0,x0,lrs_.super().permutationInverse());
			for (int k=sSuper.getRowPtr(i);k<sSuper.getRowPtr(i+1);k++) {
				size_t col = sSuper.getCol(k);
				ComplexOrRealType val = sSuper.getValue(k);
				if (isAlmostZero(val)) continue;
				size_t xprime=0,yprime=0;
				pack1.unpack(xprime,yprime,lrs_.super().permutation(col));


				reflectedLeft_.pushCol(xprime);
				reflectedLeft_.pushValue(val);

				reflectedRight_.pushCol(yprime);
				reflectedRight_.pushValue(val);
				counter++;
			}
		}
		reflectedLeft_.setRow(ns,counter);
		reflectedRight_.setRow(ns,counter);
	}

//	size_t findReflected(size_t x) const
//	{
//		if (s_.rank()==0) return x;
//		for (size_t i=0;i<s_.rank();i++) {
//			for (int k=s_.getRowPtr(i);k<s_.getRowPtr(i+1);k++) {
//				size_t col = s_.getCol(k);
//				if (col==x) return i;
//			}
//		}
//		std::string s(__FILE__);
//		s += " findReflected\n";
//		throw std::runtime_error(s.c_str());
//	}

	void setGs(VectorType& gs,const VectorType& v,size_t rank,size_t offset) const
	{
		VectorType gstmp(rank,0);

		for (size_t i=0;i<v.size();i++) {
			gstmp[i+offset]=v[i];
		}
		SparseMatrixType rT;
		transposeConjugate(rT,transform_);
		multiply(gs,rT,gstmp);
	}

	void computeItems(std::vector<ItemType>& items,
			  SparseMatrixType& sSector)
	{
		printFullMatrix(sSector,"sSector");

		for (size_t i=0;i<sSector.rank();i++) {
			std::vector<ComplexOrRealType> v(sSector.rank(),0);
			int equalCounter=0;
			for (int k=sSector.getRowPtr(i);k<sSector.getRowPtr(i+1);k++) {
				size_t col = sSector.getCol(k);
				if (isAlmostZero(sSector.getValue(k))) continue;
				v[col] =  sSector.getValue(k);
				if (col==i) equalCounter++;
				else equalCounter--;
			}
			if (equalCounter==1) {
				ItemType item(i); //v[i]);
				items.push_back(item);
				continue;
			}

			// add plus
			ItemType item(ItemType::PLUS,i,v);
			items.push_back(item);
			// add minus
			ItemType item2(ItemType::MINUS,i,v);
			items.push_back(item2);
		}
	}

	void setTransform(const std::vector<ItemType>& buffer)
	{
		transform_.resize(buffer.size());
		assert(buffer.size()==transform_.rank());
		size_t counter = 0;
		size_t row = 0;

		for (size_t i=0;i<buffer.size();i++) {
			if (buffer[i].type()==ItemType::MINUS) continue;
			transform_.setRow(row++,counter);
			buffer[i].setTransformPlus(transform_,counter);
		}

		for (size_t i=0;i<buffer.size();i++) {
			if (buffer[i].type()!=ItemType::MINUS) continue;
			transform_.setRow(row++,counter);
			buffer[i].setTransformMinus(transform_,counter);
		}
		transform_.setRow(transform_.rank(),counter);
	}

	void makeUnique(std::vector<ItemType>& dest,std::vector<ItemType>& src)
	{
		size_t zeros=0;
		size_t pluses=0;
		size_t minuses=0;
		for (size_t i=0;i<src.size();i++) {
			src[i].postFix();
		}

		for (size_t i=0;i<src.size();i++) {
			ItemType item = src[i];
			int x =  PsimagLite::isInVector(dest,item);
			if (x>=0) continue;
//				if (item.type ==ItemType::PLUS) {
//					size_t i = item.i;
//					size_t j = item.j;
//					ItemType item2(j,i,ItemType::PLUS);
//					x = PsimagLite::isInVector(dest,item2);
//					if (x>=0) continue;
//				}
			if (item.type()==ItemType::DIAGONAL) zeros++;
			if (item.type()==ItemType::PLUS) pluses++;
			if (item.type()==ItemType::MINUS) minuses++;

			dest.push_back(item);
		}
		std::ostringstream msg;
		msg<<pluses<<" +, "<<minuses<<" -, "<<zeros<<" zeros.";
		progress_.printline(msg,std::cout);
		plusSector_ = zeros + pluses;
	}

	void printFullMatrix(const SparseMatrixType& s,const std::string& name) const
	{
		PsimagLite::Matrix<ComplexOrRealType> fullm(s.rank(),s.rank());
		crsMatrixToFullMatrix(fullm,s);
		std::cout<<"--------->   "<<name<<" <----------\n";
		symbolicPrint(std::cout,fullm);
		//std::cout<<fullm;

	}

	void split(SparseMatrixType& matrixA,SparseMatrixType& matrixB,const SparseMatrixType& matrix) const
	{
		size_t counter = 0;
		matrixA.resize(plusSector_);
		for (size_t i=0;i<plusSector_;i++) {
			matrixA.setRow(i,counter);
			for (int k=matrix.getRowPtr(i);k<matrix.getRowPtr(i+1);k++) {
				size_t col = matrix.getCol(k);
				ComplexOrRealType val = matrix.getValue(k);
				if (col<plusSector_) {
					matrixA.pushCol(col);
					matrixA.pushValue(val);
					counter++;
					continue;
				}
				if (!isAlmostZero(val)) {
					std::string s(__FILE__);
					s += " Hamiltonian has no reflection symmetry.";
					throw std::runtime_error(s.c_str());
				}
			}
		}
		matrixA.setRow(plusSector_,counter);

		size_t rank = matrix.rank();
		size_t minusSector=rank-plusSector_;
		matrixB.resize(minusSector);
		counter=0;
		for (size_t i=plusSector_;i<rank;i++) {
			matrixB.setRow(i-plusSector_,counter);
			for (int k=matrix.getRowPtr(i);k<matrix.getRowPtr(i+1);k++) {
				size_t col = matrix.getCol(k);
				ComplexOrRealType val = matrix.getValue(k);
				if (col>=plusSector_) {
					matrixB.pushCol(col-plusSector_);
					matrixB.pushValue(val);
					counter++;
					continue;
				}
				if (!isAlmostZero(val)) {
					std::string s(__FILE__);
					s += " Hamiltonian has no reflection symmetry.";
					throw std::runtime_error(s.c_str());
				}
			}
		}
		matrixB.setRow(minusSector,counter);
	}

	//SparseMatrixType& operator()() const { return s_; }

	const LeftRightSuperType& lrs_;
	size_t n0_; // hilbert size of one site
	PsimagLite::ProgressIndicator progress_;
	size_t plusSector_;
	bool isEnabled_;
	size_t expandSys_;
	SparseMatrixType reflectedLeft_,reflectedRight_;
	SparseMatrixType transform_;
}; // class ReflectionOperator

} // namespace Dmrg 

/*@}*/
#endif // REFLECTION_OPERATOR_H
