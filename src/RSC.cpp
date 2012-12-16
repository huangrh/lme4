// -*- mode: C++; c-indent-level: 4; c-basic-offset: 4; tab-width: 8 -*-
//
// RSC.cpp: Implementation of the regular sparse matrix representation
//
// Copyright (C) 2012-2013 Douglas Bates, Martin Maechler and Ben Bolker
//
// This file is part of lme4.

#include "RSC.h"
#include "CHM.h"

using namespace Rcpp;

namespace lme4 {
    RSC::RSC(const SEXP rvSEXP, const SEXP xvSEXP, const SEXP lowerSEXP)
	: rv(rvSEXP),
	  xv(xvSEXP),
	  lower(lowerSEXP),
	  k(rv.nrow()),
	  kpp(xv.nrow()),
	  n(xv.ncol()),
	  p(kpp - k),
	  q((*std::max_element(rv.begin(), rv.end())) + 1) {
	Rcout << "k = " << k << ", kpp = " << kpp << ", n = " << n
	      << ", p = " << p << ", q = " << q << std::endl;
	if (rv.ncol() != n || p < 0) stop("dimension mismatch of rv and xv");
	if (k != std::count_if(lower.begin(), lower.end(), ::R_finite))
	    stop("dimension mismatch of rv and lower");
	if (*std::min_element(rv.begin(), rv.end()) != 0)
	    stop("minimum row index must be 0");
    }

    NumericVector &RSC::apply_lambda(const NumericVector &theta,
				     NumericVector &dest) const {
	int dpos(-1);
	double *rr(0);		// initialize to NULL pointer
	for (int k = 0; k < kpp; ++k) {
	    if (lower[k] == 0.) { // diagonal element of a factor
		dest[++dpos] *= theta[k];
		rr = &dest[k];
	    }
	    else dest[dpos] += theta[k] * *++rr;
	}
	return dest;
    }

    void RSC::update_A(const NumericVector &theta, const NumericVector &resid,
		       S4 &AA, NumericVector &ubeta) const {
	if (theta.size() != lower.size())
	    stop("Dimension mismatch of theta and lower");
	if (resid.size() != n) stop("Dimension of resid should be n");
	if (ubeta.size() != q + p) stop("Dimension of ubeta should be q + p");
	if (!AA.is("dsCMatrix")) stop("A must be a \"dsCMatrix\" object");
	dsCMatrix A(AA);
	if (A.nrow() != q + p) stop("size of A must be q + p");
	const IntegerVector &rowval(A.rowval()), &colptr(A.colptr());
	NumericVector& nzval(A.nzval());
	NumericVector w(kpp);
	// initializations
	std::fill(nzval.begin(), nzval.end(), double(0)); // zero the contents of A
	std::fill(ubeta.begin(), ubeta.end(), double(0)); // and ubeta
	for (int i = 0; i < q; ++i) {  // initialize Z part of A to the identity
	    int ll(colptr[i + 1] - 1); // index of last element in column i
	    int ii(rowval[ll]);	   // should be the diagonal element
	    if (ii != i) stop("A is not stored as the upper triangle");
	    nzval[ii] = 1.;
	}
	for (int j = 0; j < n; ++j)	{ // iterate over columns of ZtXt
	    double rj(resid[j]);
	    std::copy(&(xv(0,j)), &(xv(0,j)) + kpp, w.begin()); // copy j'th column
	    apply_lambda(theta, w);
	    for (int i = 0; i < kpp; ++i) ubeta[rv(i,j)] += rj * w[i];
	    // scan up the j'th column of ZtXt, which makes
	    // it easier to evaluate the upper triangle of A
	    for (int i = kpp - 1; i >= 0; --i) {
		int ii(rv(i, j));	// row of ZtXt (column of m) 
		int cpi(colptr[ii]), ll(colptr[ii + 1] - 1); // ll should be location of diagonal
		if (rowval[ll] != ii) stop("u is not upper triangular");
		nzval[ll] += w[i] * w[i];
		for (int l = i - 1; l >= 0 && ll >= cpi; --l) { // off-diagonals in m
		    int ii1(rv(l,j)); // row in ZtXt
		    while (rowval[ll] > ii1) --ll; // up to the desired row
		    if (rowval[ll] != ii1) stop("Pattern mismatch");
		    nzval[ll] += w[i] * w[l];
		}
	    }
	}
    }

}
