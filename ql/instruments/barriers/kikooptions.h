/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003, 2004 Neil Firth
 Copyright (C) 2003, 2004 Ferdinando Ametrano
 Copyright (C) 2003, 2004, 2007 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib License.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied Warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file barrieroption.hpp
    \brief Barrier option on a single asset
*/

#ifndef quantlib_kiko_option_hpp
#define quantlib_kiko_option_hpp

#include <ql/instruments/barriers/kikobarrier.h>
#include <ql/instruments/barriertype.hpp>
#include <ql/instruments/dividendschedule.hpp>
#include <ql/instruments/oneassetoption.hpp>
#include <ql/instruments/payoffs.hpp>

namespace QuantLib {

    class GeneralizedBlackScholesProcess;

    //! %Barrier option on a single asset.
    /*! The analytic pricing engine will be used if none is passed.

        \ingroup instruments
    */
    class KikoOption : public OneAssetOption {
      public:
        class arguments;
        class engine;

        /* self-developed constructor */
        KikoOption(const ext::shared_ptr<KIKOBarrier>& barrier,
                   const ext::shared_ptr<StrikedTypePayoff>& payoff,
                   const ext::shared_ptr<Exercise>& exercise);

        KikoOption(const ext::shared_ptr<KIKOBarrier>& barrier,
                   const ext::shared_ptr<StrikedTypePayoff>& lowerpayoff,
                   const ext::shared_ptr<StrikedTypePayoff>& upperpayoff,
                   const ext::shared_ptr<Exercise>& exercise);

        void setupArguments(PricingEngine::arguments*) const override;

      protected:
        // arguments
        ext::shared_ptr<KIKOBarrier> kikoBarrier_;

        ext::shared_ptr<StrikedTypePayoff> lowerpayoff_;
        ext::shared_ptr<StrikedTypePayoff> upperpayoff_;
    };

    //! %Arguments for barrier option calculation
    class KikoOption::arguments : public OneAssetOption::arguments {
      public:
        arguments();
        ext::shared_ptr<KIKOBarrier> kikoBarrier_;
        ext::shared_ptr<StrikedTypePayoff> lowerpayoff_;
        ext::shared_ptr<StrikedTypePayoff> upperpayoff_;
        void validate() const override;
    };

    //! %Barrier-option %engine base class
    class KikoOption::engine : public GenericEngine<KikoOption::arguments, KikoOption::results> {};

}

#endif